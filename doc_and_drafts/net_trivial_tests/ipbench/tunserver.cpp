/**
Copyrighted (C) 2016, GPL v3 Licence (may include also other code)
See LICENCE.txt
*/

/*

TODO(r) do not tunnel entire (encrypted) copy of TUN, trimm it from headers that we do not need
TODO(r) establish end-to-end AE (cryptosession)

*/

const char * disclaimer = "*** WARNING: This is a work in progress, do NOT use this code, it has bugs, vulns, and 'typpos' everywhere! ***"; // XXX

#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/program_options.hpp>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>

#include <string.h>
#include <assert.h>

#include <thread>

#include <cstring>

#include <sodium.h>

#include "libs1.hpp"
#include "counter.hpp"
#include "cpputils.hpp"

// linux (and others?) select use:
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

// for low-level Linux-like systems TUN operations
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include<netinet/ip_icmp.h>   //Provides declarations for icmp header
#include<netinet/udp.h>   //Provides declarations for udp header
#include<netinet/tcp.h>   //Provides declarations for tcp header
#include<netinet/ip.h>    //Provides declarations for ip header
// #include <net/if_ether.h> // peer over eth later?
// #include <net/if_media.h> // ?

#include "../NetPlatform.h" // from cjdns

// #include <net/if_tap.h>
#include <linux/if_tun.h>

#include "c_ip46_addr.hpp"
#include "c_peering.hpp"

// ------------------------------------------------------------------

void error(const std::string & msg) {
	std::cout << "Error: " << msg << std::endl;
	throw std::runtime_error(msg);
}

// ------------------------------------------------------------------



namespace developer_tests {

bool wip_strings_encoding(boost::program_options::variables_map & argm) {
	_mark("Tests of string encoding");
	string s1,s2,s3;
	using namespace std;
	s1="4a4b4c4d4e"; // in hex
//	s2="ab"; // in b64
	s3="y"; // in bin


	// TODO assert is results are as expected!
	// TODO also assert that the exceptions are thrown as they should be, below

	auto s1_hex = string_as_hex( s1 );
	c_haship_pubkey pub1( s1_hex );
	_info("pub = " << to_string(pub1));
	_info("pub = " << to_string(c_haship_pubkey(string_as_hex("4"))));
	_info("pub = " << to_string(c_haship_pubkey(string_as_hex("f4b4c4d4e"))));
	_info("pub = " << to_string(c_haship_pubkey(string_as_hex("4a4b4c4d4e"))));
	_info("pub = " << to_string(c_haship_pubkey(string_as_hex(""))));
	_info("pub = " << to_string(c_haship_pubkey(string_as_hex("ffffffff"))));
	_info("pub = " << to_string(c_haship_pubkey(string_as_hex("00000000"))));
	try {
		_info("pub = " << to_string(c_haship_pubkey(string_as_hex("4a4b4c4d4eaba46381826328363782917263521719badbabdbadfade7455467383947543473839474637293474637239273534873"))));
	} catch (std::exception &e) { _note("Test failed, as expected: " << e.what()); }
	try {
		_info("pub = " << to_string(c_haship_pubkey(string_as_hex("0aq"))));
	} catch (std::exception &e) { _note("Test failed, as expected: " << e.what()); }
	try {
		_info("pub = " << to_string(c_haship_pubkey(string_as_hex("qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"))));
	} catch (std::exception &e) { _note("Test failed, as expected: " << e.what()); }

//	c_haship_pubkey pub2( string_as_b64( s1 ) );
//	c_haship_pubkey pub3( string_as_bin( s1 ) );

	_info("Test done");
	return false;
}

} // namespace

// ------------------------------------------------------------------

class c_routing_manager { ///< holds nowledge about routes, and searches for new ones
	public: // make it private, when possible - e.g. when all operator<< are changed to public: print(ostream&) const;
		enum t_route_state { e_route_state_found, e_route_state_dead };

		enum t_search_mode {  // why we look for a route
			e_search_mode_route_own_packet, // we want ourselves to send there
			e_search_mode_route_other_packet,  // we want to route packet of someone else
			e_search_mode_help_find }; // some else is asking us about the route

		typedef	std::chrono::steady_clock::time_point t_route_time; ///< type for representing times using in routing search etc

		class c_route_info {
			public:
				t_route_state m_state; ///< e.g. e_route_state_found is route is ready to be used
				c_haship_addr m_nexthop; ///< hash-ip of next hop in this route

				int m_cost; ///< some general cost
				t_route_time m_time; ///< age of this route
				int m_ttl; ///< at which TTL we got this reply
		};

		class c_route_reason {
			public:
				c_haship_addr m_his_addr; ///< his address to which we should tell him the path
				t_search_mode m_search_mode; ///< do we search it for him because we need to route for him, or because he asked, etc
				// c_haship_addr m_his_question; ///< the address about which we aksed

				c_route_reason(c_haship_addr his_addr, t_search_mode mode);

				bool operator<(const c_route_reason &other) const;
				bool operator==(const c_route_reason &other) const;
		};

		class c_route_reason_detail {
			public:
				t_route_time m_when; ///< when we hasked about this address last time
				c_route_reason_detail( t_route_time when );
		};

		class c_route_search {
			public:
				c_haship_addr m_addr; ///< goal of search: dst address
				bool m_ever; ///< was this ever actually searched yet
				t_route_time m_ask_time; ///< at which time we last time tried asking
				int m_ask_ttl; ///< at which TTL we last time tried asking

				map< c_route_reason , c_route_reason_detail > m_request; ///< information about all other people who are asking about this address

				c_route_search(c_haship_addr addr);

				void add_request(c_routing_manager::c_route_reason reason); ///< add info that this gy also wants to be informed about the path
		};

		// searches:
		typedef std::map< c_haship_addr, unique_ptr<c_route_search> > t_route_search_by_dst; ///< running searches, by the hash-ip of finall destination
		t_route_search_by_dst m_search; ///< running searches

		// known routes:
		typedef std::map< c_haship_addr, unique_ptr<c_route_info> > t_route_nexthop_by_dst; ///< routes to destinations: the hash-ip of next hop, by hash-ip of finall destination
		t_route_nexthop_by_dst m_route_nexthop; ///< known routes: the hash-ip of next hop, indexed by hash-ip of finall destination

	public:
		c_haship_addr get_route_nexthop(c_haship_addr dst, c_routing_manager::c_route_reason reason, bool start_search=true);
};

std::ostream & operator<<(std::ostream & ostr, std::chrono::steady_clock::time_point) {
	return ostr << "(chrono TODO)"; // TODO(u)
}

std::ostream & operator<<(std::ostream & ostr, const c_routing_manager::t_search_mode & obj) {
	switch (obj) {
		case c_routing_manager::e_search_mode_route_own_packet: return ostr<<"route_OWN";
		case c_routing_manager::e_search_mode_route_other_packet: return ostr<<"route_OTHER";
		case c_routing_manager::e_search_mode_help_find: return ostr<<"help_FIND";
	}
	_warn("Unknown reason"); return ostr<<"???";
}

std::ostream & operator<<(std::ostream & ostr, const c_routing_manager::c_route_reason & obj) {
	return ostr << "{Reason: asked from " << obj.m_his_addr << " as " << obj.m_search_mode << "}";
}
std::ostream & operator<<(std::ostream & ostr, const c_routing_manager::c_route_reason_detail & obj) {
	return ostr << "{Reason...: at " << obj.m_when << "}";
}

std::ostream & operator<<(std::ostream & ostr, const c_routing_manager::c_route_search & obj) {
	ostr << "{SEARCH for route to DST="<<obj.m_addr<<", was yet run=" << (obj.m_ever?"YES":"never")
		<< " ask: time="<<obj.m_ask_time<<" ttl="<<obj.m_ask_ttl;
	if (obj.m_request.size()) {
		ostr << "with " << obj.m_request.size() << " REQUESTS:" << endl;
		for(auto const & r : obj.m_request) ostr << " REQ: " << r.first << " => " << r.second << endl;
		ostr << endl;
	} else ostr << " (no requesters here)";
	ostr << "}";
	return ostr;
}

c_routing_manager::c_route_reason_detail::c_route_reason_detail( t_route_time when )
	: m_when(when)
{ }

void c_routing_manager::c_route_search::add_request(c_routing_manager::c_route_reason reason) {
	auto found = m_request.find( reason );
	if (found == m_request.end()) { // new reason for search
		c_route_reason_detail reason_detail( std::chrono::steady_clock::now() );
		_info("Adding new reason for search: " << reason << " details: " << reason_detail);
		m_request.emplace(reason, reason_detail);
	}
	else {
		auto & detail = found->second;
		_info("Updating reason of search: " << reason << " old detail: " << detail );
		detail.m_when = std::chrono::steady_clock::now();
		_info("Updating reason of search: " << reason << " new detail: " << detail );
	}
}

c_routing_manager::c_route_reason::c_route_reason(c_haship_addr his_addr, t_search_mode mode)
	: m_his_addr(his_addr), m_search_mode(mode)
{ 
	_info("NEW reason: "<< (*this));
}

bool c_routing_manager::c_route_reason::operator<(const c_route_reason &other) const {
	if (this->m_his_addr < other.m_his_addr) return 1;
	if (this->m_search_mode < other.m_search_mode) return 1;
	return 0;
}

bool c_routing_manager::c_route_reason::operator==(const c_route_reason &other) const {
	return (this->m_his_addr == other.m_his_addr) && (this->m_search_mode == other.m_search_mode);
}

c_routing_manager::c_route_search::c_route_search(c_haship_addr addr) 
	: m_addr(addr), m_ever(false), m_ask_time(), m_ask_ttl(2)
{ 
	_info("NEW router SEARCH: " << (*this));
}

c_haship_addr c_routing_manager::get_route_nexthop(c_haship_addr dst, c_routing_manager::c_route_reason reason, bool start_search) {
	_info("ROUTING-MANAGER: find: " << dst << ", for reason: " << reason );
	auto found = m_route_nexthop.find( dst );
	if (found != m_route_nexthop.end()) { // found
		auto nexthop = found->second->m_nexthop;
		_info("ROUTING-MANAGER: found: " << nexthop);
		return nexthop; // <---
	} 
	else { // don't have a planned route to him
		if (!start_search) { 
			_info("No route, but we also so not want to search for it.");
			throw std::runtime_error("no route known (and we do NOT WANT TO search) to dst=" + STR(dst));
		}
		else {
			_info("Route not found, we will be searching");
			auto search_iter = m_search.find(dst);
			if (search_iter == m_search.end()) {
				_info("STARTED (created) a new SEARCH for this route to dst="<<dst);
				auto new_search = make_unique<c_route_search>(dst);
				new_search->add_request( reason );
				m_search.emplace( std::move(dst) , std::move(new_search) );
			}
			else {
				_info("STARTED (updated) an existing SEARCH for this route to dst="<<dst);
				search_iter->second->add_request( reason ); // add reason
			}
		}
	}
	throw std::runtime_error("no route known (yet) to dst=" + STR(dst));
}


// ------------------------------------------------------------------

class c_tunserver {
	public:
		c_tunserver();
		void configure_mykey_from_string(const std::string &mypub, const std::string &mypriv);
		void run(); ///< run the main loop
		void add_peer(const t_peering_reference & peer_ref); ///< add this as peer
		void add_peer_simplestring(const string & simple); ///< add this as peer, from a simple string like "ip-pub" TODO(r) instead move that to ctor of t_peering_reference
		void set_my_name(const string & name); ///< set a nice name of this peer (shown in debug for example)

		void help_usage() const; ///< show help about usage of the program

		typedef enum {
			e_route_method_from_me=1, ///< I am the oryginal sender (try hard to send it)
			e_route_method_if_direct_peer=2, ///< Send data only if if I know the direct peer (e.g. I just route it for someone else - in star protocol the center node)
			e_route_method_default=3, ///< The default routing method
		} t_route_method;

	protected:
		void prepare_socket(); ///< make sure that the lower level members of handling the socket are ready to run
		void event_loop(); ///< the main loop
		void wait_for_fd_event(); ///< waits for event of I/O being ready, needs valid m_tun_fd and others, saves the fd_set into m_fd_set_data

		std::pair<c_haship_addr,c_haship_addr> parse_tun_ip_src_dst(const char *buff, size_t buff_size, unsigned char ipv6_offset); ///< from buffer of TUN-format, with ipv6 bytes at ipv6_offset, extract ipv6 (hip) destination
		std::pair<c_haship_addr,c_haship_addr> parse_tun_ip_src_dst(const char *buff, size_t buff_size); ///< the same, but with ipv6_offset that matches our current TUN

		///@brief push the tunneled data to where they belong. On failure returns false or throws, true if ok. 
		bool route_tun_data_to_its_destination(t_route_method method, const char *buff, size_t buff_size, c_routing_manager::c_route_reason reason); 

		///@brief more advanced version for use in routing
		bool route_tun_data_to_its_destination(t_route_method method, const char *buff, size_t buff_size, c_routing_manager::c_route_reason reason, c_haship_addr next_hip, int recurse_level);  

		void peering_ping_all_peers();
		void debug_peers();

	private:
		string m_my_name; ///< a nice name, see set_my_name
		int m_tun_fd; ///< fd of TUN file
		unsigned char m_tun_header_offset_ipv6; ///< current offset in TUN/TAP data to the position of ipv6

		int m_sock_udp; ///< the main network socket (UDP listen, send UDP to each peer)

		fd_set m_fd_set_data; ///< select events e.g. wait for UDP peering or TUN input

		typedef std::map< c_haship_addr, unique_ptr<c_peering> > t_peers_by_haship; ///< peers (we always know their IPv6 - we assume here), indexed by their hash-ip
		t_peers_by_haship m_peer; ///< my peers, indexed by their hash-ip

		c_haship_pubkey m_haship_pubkey; ///< pubkey of my IP
		c_haship_addr m_haship_addr; ///< my haship addres
		c_peering & find_peer_by_sender_peering_addr( c_ip46_addr ip ) const ;

		c_routing_manager m_routing_manager; ///< the routing engine used for most things
};

// ------------------------------------------------------------------

using namespace std; // XXX move to implementations, not to header-files later, if splitting cpp/hpp

void c_tunserver::add_peer_simplestring(const string & simple) {
	size_t pos1 = simple.find('-');
	string part_ip = simple.substr(0,pos1);
	string part_pub = simple.substr(pos1+1);
	_note("Simple string parsed as: " << part_ip << " and " << part_pub );
	this->add_peer( t_peering_reference( part_ip, string_as_hex( part_pub ) ) );
}

c_tunserver::c_tunserver()
 : m_my_name("unnamed-tunserver"), m_tun_fd(-1), m_tun_header_offset_ipv6(0), m_sock_udp(-1)
{
}

void c_tunserver::set_my_name(const string & name) {  m_my_name = name; _note("This node is now named: " << m_my_name);  }

// my key
void c_tunserver::configure_mykey_from_string(const std::string &mypub, const std::string &mypriv) {
	m_haship_pubkey = string_as_bin( string_as_hex( mypub ) );
	m_haship_addr = c_haship_addr( c_haship_addr::tag_constr_by_hash_of_pubkey() , m_haship_pubkey );
	_info("Configuring the router, I am: pubkey="<<to_string(m_haship_pubkey)<<" ip="<<to_string(m_haship_addr));
}

// add peer
void c_tunserver::add_peer(const t_peering_reference & peer_ref) { ///< add this as peer
	_note("Adding peer from reference=" << peer_ref
		<< " that reads: " << "peering-address=" << peer_ref.peering_addr << " pubkey=" << to_string(peer_ref.pubkey) << " haship_addr=" << to_string(peer_ref.haship_addr) );
	auto peering_ptr = make_unique<c_peering_udp>(peer_ref);
	// TODO(r) check if duplicated peer (map key) - warn or ignore dep on parameter
	m_peer.emplace( std::make_pair( peer_ref.haship_addr ,  std::move(peering_ptr) ) );
}

void c_tunserver::help_usage() const {
	// TODO(r) remove, using boost options
}

void c_tunserver::prepare_socket() {
	m_tun_fd = open("/dev/net/tun", O_RDWR);
	assert(! (m_tun_fd<0) );

  as_zerofill< ifreq > ifr; // the if request
	ifr.ifr_flags = IFF_TUN; // || IFF_MULTI_QUEUE; TODO
	strncpy(ifr.ifr_name, "galaxy%d", IFNAMSIZ);

	auto errcode_ioctl =  ioctl(m_tun_fd, TUNSETIFF, (void *)&ifr);
	m_tun_header_offset_ipv6 = g_tuntap::TUN_with_PI::header_position_of_ipv6; // matching the TUN/TAP type above
	if (errcode_ioctl < 0)_throw( std::runtime_error("Error in ioctl")); // TODO

	_mark("Allocated interface:" << ifr.ifr_name);

	{
		uint8_t address[16];
		for (int i=0; i<16; ++i) address[i] = m_haship_addr.at(i);
		// TODO: check if there is no race condition / correct ownership of the tun, that the m_tun_fd opened above is...
		// ...to the device to which we are setting IP address here:
		assert(address[0] == 0xFD);
		assert(address[1] == 0x42);
		NetPlatform_addAddress(ifr.ifr_name, address, 16, Sockaddr_AF_INET6);
	}

	// create listening socket
	m_sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
	_assert(m_sock_udp >= 0);

	int port = 9042;
	c_ip46_addr address_for_sock = c_ip46_addr::any_on_port(port);

	{
		int bind_result = -1;
		if (address_for_sock.get_ip_type() == c_ip46_addr::t_tag::tag_ipv4) {
			sockaddr_in addr4 = address_for_sock.get_ip4();
			bind_result = bind(m_sock_udp, reinterpret_cast<sockaddr*>(&addr4), sizeof(addr4));  // reinterpret allowed by Linux specs
		}
		else if(address_for_sock.get_ip_type() == c_ip46_addr::t_tag::tag_ipv6) {
			sockaddr_in6 addr6 = address_for_sock.get_ip6();
			bind_result = bind(m_sock_udp, reinterpret_cast<sockaddr*>(&addr6), sizeof(addr6));  // reinterpret allowed by Linux specs
		}
			_assert( bind_result >= 0 ); // TODO change to except
			_assert(address_for_sock.get_ip_type() != c_ip46_addr::t_tag::tag_none);
	}
	_info("Bind done - listening on UDP on: "); // TODO  << address_for_sock
}

void c_tunserver::wait_for_fd_event() { // wait for fd event
	_info("Selecting");
	// set the wait for read events:
	FD_ZERO(& m_fd_set_data);
	FD_SET(m_sock_udp, &m_fd_set_data);
	FD_SET(m_tun_fd, &m_fd_set_data);

	auto fd_max = std::max(m_tun_fd, m_sock_udp);
	_assert(fd_max < std::numeric_limits<decltype(fd_max)>::max() -1); // to be more safe, <= would be enough too
	_assert(fd_max >= 1);

	timeval timeout { 1 , 0 }; // http://pubs.opengroup.org/onlinepubs/007908775/xsh/systime.h.html

	auto select_result = select( fd_max+1, &m_fd_set_data, NULL, NULL, & timeout); // <--- blocks
	_assert(select_result >= 0);
}

std::pair<c_haship_addr,c_haship_addr> c_tunserver::parse_tun_ip_src_dst(const char *buff, size_t buff_size) { ///< the same, but with ipv6_offset that matches our current TUN
	return parse_tun_ip_src_dst(buff,buff_size, m_tun_header_offset_ipv6 );
}

std::pair<c_haship_addr,c_haship_addr> c_tunserver::parse_tun_ip_src_dst(const char *buff, size_t buff_size, unsigned char ipv6_offset) {
	// vuln-TODO(u) throw on invalid size + assert

	size_t pos_src = ipv6_offset + g_ipv6_rfc::header_position_of_src , len_src = g_ipv6_rfc::header_length_of_src;
	size_t pos_dst = ipv6_offset + g_ipv6_rfc::header_position_of_dst , len_dst = g_ipv6_rfc::header_length_of_dst;
	assert(buff_size > pos_src+len_src);
	assert(buff_size > pos_dst+len_dst);
	// valid: reading pos_src up to +len_src, and same for dst

	char ipv6_str[INET6_ADDRSTRLEN]; // for string e.g. "fd42:ffaa:..."

	memset(ipv6_str, 0, INET6_ADDRSTRLEN);
	inet_ntop(AF_INET6, buff + pos_src, ipv6_str, INET6_ADDRSTRLEN); // ipv6 octets from 8 is source addr, from ipv6 RFC
	_dbg1("src ipv6_str " << ipv6_str);
	c_haship_addr ret_src(c_haship_addr::tag_constr_by_addr_string(), ipv6_str);

	memset(ipv6_str, 0, INET6_ADDRSTRLEN);
	inet_ntop(AF_INET6, buff + pos_dst, ipv6_str, INET6_ADDRSTRLEN); // ipv6 octets from 24 is destination addr, from
	_dbg1("dst ipv6_str " << ipv6_str);
	c_haship_addr ret_dst(c_haship_addr::tag_constr_by_addr_string(), ipv6_str);

	return std::make_pair( ret_dst , ret_src );
}

void c_tunserver::peering_ping_all_peers() {
	_info("Sending ping to all peers");
	for(auto & v : m_peer) { // to each peer
		auto & target_peer = v.second;
		auto peer_udp = unique_cast_ptr<c_peering_udp>( target_peer ); // upcast to UDP peer derived

		// [protocol] build raw
		string_as_bin cmd_data;
		cmd_data.bytes += string_as_bin( m_haship_pubkey ).bytes;
		cmd_data.bytes += ";";
		peer_udp->send_data_udp_cmd(c_protocol::e_proto_cmd_public_hi, cmd_data, m_sock_udp);
	}
}

void c_tunserver::debug_peers() {
	_note("=== Debug peers ===");
	for(auto & v : m_peer) { // to each peer
		auto & target_peer = v.second;
		_info("  * Known peer on key [ " << string_as_dbg( v.first ).get() << " ] => " << (* target_peer) );
	}
}

bool c_tunserver::route_tun_data_to_its_destination(t_route_method method, const char *buff, size_t buff_size, c_routing_manager::c_route_reason reason, 
	c_haship_addr next_hip, int recurse_level) 
{
	// --- choose next hop in peering ---

	// try direct peers:
	auto peer_it = m_peer.find(next_hip); // find c_peering to send to
	if (peer_it == m_peer.end()) { // not a direct peer!
		if (recurse_level>1) {
			_warn("DROP: Recruse level too big in choosing peer");
			return false; // <---
		}
		c_haship_addr via_hip;
		try {
			auto via_hip = m_routing_manager.get_route_nexthop(next_hip , reason , true);
		} catch(...) { _info("ROUTE MANAGER: can not find route"); return false; }
		_info("Route found via hip: via_hip = " << via_hip);
		bool ok = this->route_tun_data_to_its_destination(method, buff, buff_size, reason,  via_hip, recurse_level+1);
		if (!ok) { _info("Routing failed"); return false; } // <---
		_info("Routing seems to succeed");
	}
	else { // next_hip is a direct peer, send to it:
		auto & target_peer = peer_it->second;
		_info("ROUTE-PEER, selected peerig next hop is: " << (*target_peer) );
		auto peer_udp = unique_cast_ptr<c_peering_udp>( target_peer ); // upcast to UDP peer derived
		peer_udp->send_data_udp(buff, buff_size, m_sock_udp); // <--- ***
	}
	return true;
}

bool c_tunserver::route_tun_data_to_its_destination(t_route_method method, const char *buff, size_t buff_size, c_routing_manager::c_route_reason reason) {
	try {
		c_haship_addr dst_hip = parse_tun_ip_src_dst(buff, buff_size).second;
		_info("Destination HIP:" << dst_hip);
		bool ok = this->route_tun_data_to_its_destination(method, buff, buff_size, reason,  dst_hip, 0);
		if (!ok) { _info("Routing/sending failed (top level)"); return false; }
	} catch(std::exception &e) {
		_warn("Can not send to peer, because:" << e.what()); // TODO more info (which peer, addr, number)
	} catch(...) {
		_warn("Can not send to peer (unknown)"); // TODO more info (which peer, addr, number)
	}
	_info("Routing/sending OK (top level)"); 
	return true;
}

c_peering & c_tunserver::find_peer_by_sender_peering_addr( c_ip46_addr ip ) const {
	for(auto & v : m_peer) { if (v.second->m_peering_addr == ip) return * v.second.get(); }
	throw std::runtime_error("We do not know a peer with such IP=" + STR(ip));
}


void c_tunserver::event_loop() {
	_info("Entering the event loop");
	c_counter counter(2,true);
	c_counter counter_big(10,false);

	fd_set fd_set_data;


	this->peering_ping_all_peers();
	auto ping_all_time_last = std::chrono::steady_clock::now(); // last time we sent ping to all
	auto ping_all_frequency = std::chrono::seconds( 5 ); // how often to ping them


	const int buf_size=65536;
	char buf[buf_size];


	while (1) {
		auto time_now = std::chrono::steady_clock::now(); // time now
		if (time_now > ping_all_time_last + ping_all_frequency ) {
			_note("It's time to ping all peers again");
			peering_ping_all_peers(); // TODO(r) later ping only peers that need that
			ping_all_time_last = std::chrono::steady_clock::now();
		}

		debug_peers();

		{ string xx(10,'-');	std::cerr << endl << xx << " Node " << m_my_name << xx << endl << endl; } // --- print your name ---

		wait_for_fd_event();

		// TODO(r): program can be hanged/DoS with bad routing, no TTL field yet

		try { // ---

		if (FD_ISSET(m_tun_fd, &m_fd_set_data)) { // data incoming on TUN - send it out to peers
			auto size_read = read(m_tun_fd, buf, sizeof(buf)); // <-- read data from TUN
			_info("TUN read " << size_read << " bytes: [" << string(buf,size_read)<<"]");
			this->route_tun_data_to_its_destination(
				e_route_method_from_me, buf, size_read,
				c_routing_manager::c_route_reason( c_haship_addr() , c_routing_manager::e_search_mode_route_own_packet)
			); // push the tunneled data to where they belong
		}
		else if (FD_ISSET(m_sock_udp, &m_fd_set_data)) { // data incoming on peer (UDP) - will route it or send to our TUN
			sockaddr_in6 from_addr_raw; // peering address of peer (socket sender), raw format
			socklen_t from_addr_raw_size; // ^ size of it

			c_ip46_addr peer_ip; // IP of peer who sent it

			// ***
			from_addr_raw_size = sizeof(from_addr_raw); // IN/OUT parameter to recvfrom, sending it for IN to be the address "buffer" size
			auto size_read = recvfrom(m_sock_udp, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>( & from_addr_raw), & from_addr_raw_size);
			// ^- reinterpret allowed by linux specs (TODO)
			// sockaddr *src_addr, socklen_t *addrlen);

			if (from_addr_raw_size == sizeof(sockaddr_in6)) { // the message arrive from IP pasted into sockaddr_in6 format
				_erro("NOT IMPLEMENTED yet - recognizing IP of ipv6 peer"); // peeripv6-TODO(r)(easy)
				// trivial
			}
			else if (from_addr_raw_size == sizeof(sockaddr_in)) { // the message arrive from IP pasted into sockaddr_in (ipv4) format
				sockaddr_in addr = * reinterpret_cast<sockaddr_in*>(& from_addr_raw); // mem-cast-TODO(p) confirm reinterpret
				peer_ip.set_ip4(addr);
			} else {
				throw std::runtime_error("Data arrived from unknown socket address type");
			}

			_info("UDP Socket read from direct peer_ip = " << peer_ip <<", size " << size_read << " bytes: " << string_as_dbg( string_as_bin(buf,size_read)).get());
			// ------------------------------------

			if (! (size_read >= 2) ) { _warn("INVALIDA DATA, size_read="<<size_read); continue; } // !
			assert( size_read >= 2 ); // buf: reads from position 0..1 are asserted as valid now

			int proto_version = static_cast<int>( static_cast<unsigned char>(buf[0]) ); // TODO
			_assert(proto_version >= c_protocol::current_version ); // let's assume we will be backward compatible (but this will be not the case untill official stable version probably)
			c_protocol::t_proto_cmd cmd = static_cast<c_protocol::t_proto_cmd>( buf[1] );

			_info("Command is: " << cmd);

			if (cmd == c_protocol::e_proto_cmd_tunneled_data) { // [protocol] tunneled data
				c_peering & sender_as_peering = find_peer_by_sender_peering_addr( peer_ip ); // warn: returned value depends on m_peer[], do not invalidate that!!!
				_info("We recognize the sender, as: " << sender_as_peering);

				static unsigned char generated_shared_key[crypto_generichash_BYTES] = {43, 124, 179, 100, 186, 41, 101, 94, 81, 131, 17,
								198, 11, 53, 71, 210, 232, 187, 135, 116, 6, 195, 175,
								233, 194, 218, 13, 180, 63, 64, 3, 11};
				static unsigned char nonce[crypto_aead_chacha20poly1305_NPUBBYTES] = {148, 231, 240, 47, 172, 96, 246, 79};
				static unsigned char additional_data[] = {1, 2, 3};
				static unsigned long long additional_data_len = 3;
				// TODO randomize this data

				std::unique_ptr<unsigned char []> decrypted_buf (new unsigned char[size_read + crypto_aead_chacha20poly1305_ABYTES]);
				unsigned long long decrypted_buf_len;

				assert(crypto_aead_chacha20poly1305_KEYBYTES <= crypto_generichash_BYTES);

				// reinterpret the char from IO as unsigned-char as wanted by crypto code
				unsigned char * ciphertext_buf = reinterpret_cast<unsigned char*>( buf ) + 2; // TODO calculate depending on version, command, ...
				assert( size_read >= 3 );  // headers + anything
				long long ciphertext_buf_len = size_read - 2; // TODO 2 = hesder size
				assert( ciphertext_buf_len >= 1 );

				int r = crypto_aead_chacha20poly1305_decrypt(
					decrypted_buf.get(), & decrypted_buf_len,
					nullptr,
					ciphertext_buf, ciphertext_buf_len,
					additional_data, additional_data_len,
					nonce, generated_shared_key);
				if (r == -1) {
					_warn("crypto verification fails");
					continue; // skip this packet (main loop)
				}

				// TODO(r) factor out "reinterpret_cast<char*>(decrypted_buf.get()), decrypted_buf_len"

				// reinterpret for debug
				_info("UDP received, with cleartext:" << decrypted_buf_len << " bytes: [" << string( reinterpret_cast<char*>(decrypted_buf.get()), decrypted_buf_len)<<"]" );
				
				// can't wait till C++17 then with http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0144r0.pdf
				// auto { src_hip, dst_hip } = parse_tun_ip_src_dst(.....);
				c_haship_addr src_hip, dst_hip; 
				std::tie(src_hip, dst_hip) = parse_tun_ip_src_dst(reinterpret_cast<char*>(decrypted_buf.get()), decrypted_buf_len);

				if (dst_hip == m_haship_addr) { // received data addresses to us as finall destination:
					_info("UDP data is addressed to us as finall dst, sending it to TUN.");
					write(m_tun_fd, reinterpret_cast<char*>(decrypted_buf.get()), decrypted_buf_len); /// *** send the data into our TUN // reinterpret char-signess
				}
				else
				{ // received data that is addresses to someone else
					_info("UDP data is addressed to someone-else as finall dst, ROUTING it.");
					this->route_tun_data_to_its_destination(
						e_route_method_default,
						reinterpret_cast<char*>(decrypted_buf.get()), decrypted_buf_len, 
						c_routing_manager::c_route_reason( src_hip , c_routing_manager::e_search_mode_route_other_packet )
					); // push the tunneled data to where they belong // reinterpret char-signess
				}

			} // e_proto_cmd_tunneled_data
			else if (cmd == c_protocol::e_proto_cmd_public_hi) { // [protocol]
				_mark("Command HI received");
				int offset1=2; assert( size_read >= offset1);  string_as_bin cmd_data( buf+offset1 , size_read-offset1); // buf -> bin for comfortable use

				auto pos1 = 32; // [protocol] size of public key
				if (cmd_data.bytes.at(pos1)!=';') throw std::runtime_error("Invalid protocol format, missing coma"); // [protocol]
				string_as_bin bin_his_pubkey( cmd_data.bytes.substr(0,pos1) );
				_info("We received pubkey=" << string_as_dbg( bin_his_pubkey ).get() );
				t_peering_reference his_ref( peer_ip , string_as_bin( bin_his_pubkey ) );
				add_peer( his_ref );
			}
			else {
				_warn("Unknown protocol command, cmd="<<cmd);
				continue; // skip this packet (main loop)
			}
			// ------------------------------------

		}
		else _info("Idle.");

		}
		catch (std::exception &e) {
			_warn("Parsing network data caused an exception: " << e.what());
		}

// stats-TODO(r) counters
//		int sent=0;
//		counter.tick(sent, std::cout);
//		counter_big.tick(sent, std::cout);
	}
}

void c_tunserver::run() {
	std::cout << "Stating the TUN router." << std::endl;
	prepare_socket();
	event_loop();
}

// ------------------------------------------------------------------

namespace developer_tests {

string make_pubkey_for_peer_nr(int peer_nr) {
	string peer_pub = string("4a4b4c") + string("4") + string(1, char('0' + peer_nr)  );
	return peer_pub;
}

// list of peers that exist in our test-world:
struct t_peer_cmdline_ref {
	string ip;
	string pubkey;
	string privkey; ///< just in the test world. here we have knowledge of peer's secret key
};

bool wip_galaxy_route_star(boost::program_options::variables_map & argm) {
	namespace po = boost::program_options;
	const int node_nr = argm["develnum"].as<int>();  assert( (node_nr>=1) && (node_nr<=254) );
	std::cerr << "Running in developer mode - as node_nr=" << node_nr << std::endl;
	// string peer_ip = string("192.168.") + std::to_string(node_nr) + string(".62");
	int peer_nr = node_nr==1 ? 2 : 1;
	string peer_pub = make_pubkey_for_peer_nr( peer_nr );
	string peer_ip = string("192.168.") + std::to_string( peer_nr  ) + string(".62"); // each connect to node .1., except the node 1 that connects to .2.

	_mark("Developer: adding peer with arguments: ip=" << peer_ip << " pub=" << peer_pub );
	// argm.insert(std::make_pair("K", po::variable_value( int(node_nr) , false )));
	argm.insert(std::make_pair("peerip", po::variable_value( peer_ip , false )));
	argm.at("peerpub") = po::variable_value( peer_pub , false );
	argm.at("mypub") = po::variable_value( make_pubkey_for_peer_nr(node_nr)  , false );
	argm.at("myname") = po::variable_value( "testnode-" + std::to_string(node_nr) , false );
	return true; // continue the test
}

bool wip_galaxy_route_doublestar(boost::program_options::variables_map & argm) {
	namespace po = boost::program_options;
	const int my_nr = argm["develnum"].as<int>();  assert( (my_nr>=1) && (my_nr<=254) ); // number of my node
	std::cerr << "Running in developer mode - as my_nr=" << my_nr << std::endl;

	// --- define the test world ---
	map< int , t_peer_cmdline_ref > peer_to_ref; // for given peer-number - the properties of said peer as seen by us (pubkey, ip - things given on the command line)
	for (int nr=1; nr<20; ++nr) { peer_to_ref[nr] = { string("192.168.") + std::to_string( nr ) + string(".62") , string("cafe") + std::to_string(nr) ,
		string("deadbeef999fff") + std::to_string(nr) };	}

	peer_to_ref[1].pubkey = "3992967d946aee767b2ed018a6e1fc394f87bd5bfebd9ea7728edcf421d09471";
	peer_to_ref[1].privkey = "b98252fdc886680181fccd9b3c10338c04c5288477eeb40755789527eab3ba47";
	peer_to_ref[2].pubkey = "4491bfdafea313d1b354e0d993028f5e2a0a8119cc634226e5581db554d5283e";
	peer_to_ref[2].privkey = "bd48ab0e511fd5135134b9fb27491f3fdc344b29b8d8e7ce1b064d7946e48944";
	peer_to_ref[3].pubkey = "237e7a5224a8a58a0d264733380c4f3fba1f91482542afb269f382357c290445";
	peer_to_ref[3].privkey = "1bfb4bd0ac720276565f67798d069f7f4166076c6a37788ad21bae054f1b67c7";
	peer_to_ref[4].pubkey = "e27f2df89219841e0f930f7fbe000424bfbadabceb48eda2ab4521b5ce00b15c";
	peer_to_ref[4].privkey = "d73257edbfbf9200349bdc87bbc0f76f213d106f83fc027240e70c23a0f2f693";
	peer_to_ref[5].pubkey = "2cf0ab828ab1642f5fdcb8d197677f431d78fccd40d37400e1e6c51321512e66";
	peer_to_ref[5].privkey = "5d0dda56f336668e95816ccc4887c7ba23c1d14167918275e2bf76784a3ee702";
	peer_to_ref[6].pubkey = "26f4c825bcc045d7cb3ad6946d414c8ca1cbeaa3cd4738494e5308dd0d1cc053";
	peer_to_ref[6].privkey = "6c94c735dd0cfb862f991f05e3193e70b754650a5b4c998e68eb8bd1f43a15aa";
	peer_to_ref[7].pubkey = "a2047b24dfb02397a9354cc125eb9c2119a24b33c0c706f28bb184eeae064902";
	peer_to_ref[7].privkey = "2401f2be12ace34cfb221c168a7868d1d9dfe931f61feb8930799bb27fd5a253";
	// 2e83c0963e497c95bcd0bbc94b58b0c66b4c113b84fdd7587ca18e326a35c84c
	// 12fed56a2ffee2b0e3a51689ecb4048adfa4f474d31e9180d113f50fe140f5c3

	// list of connections in our test-world:
	map< int , vector<int> > peer_to_peer; // for given peer that we will create: list of his peer-number(s) that he peers into
	peer_to_peer[1] = vector<int>{ 2 , 3 };
	peer_to_peer[2] = vector<int>{ 4 , 5 };
	peer_to_peer[3] = vector<int>{ 6 , 7 };
	peer_to_peer[4] = vector<int>{ };
	peer_to_peer[5] = vector<int>{ };
	peer_to_peer[6] = vector<int>{ };
	peer_to_peer[7] = vector<int>{ };

	for (int peer_nr : peer_to_peer.at(my_nr)) { // for me, add the --peer refrence of all peers that I should peer into:
		_info(peer_nr);
		string peer_pub = peer_to_ref.at(peer_nr).pubkey;
		string peer_ip = peer_to_ref.at(peer_nr).ip;
		string peerref = peer_ip + "-" + peer_pub;
		_mark("Developer: adding peerref:" << peerref);

		vector<string> old_peer;
		try {
			old_peer = argm["peer"].as<vector<string>>();
			old_peer.push_back(peerref);
			argm.at("peer") = po::variable_value( old_peer , false );
		} catch(boost::bad_any_cast) {
			old_peer.push_back(peerref);
			argm.insert( std::make_pair("peer" , po::variable_value( old_peer , false )) );
		}
	}

	_info("Adding my keys command line");
	argm.at("mypub") = po::variable_value( peer_to_ref.at(my_nr).pubkey  , false );
	argm.at("mypriv") = po::variable_value( peer_to_ref.at(my_nr).privkey  , false );
	argm.at("myname") = po::variable_value( "testnode-" + std::to_string(my_nr) , false );

	_note("Done dev setup");
	return true;
}



} // namespace developer_tests

bool run_mode_developer(boost::program_options::variables_map & argm) {
	std::cerr << "Running in developer mode. " << std::endl;
	return developer_tests::wip_galaxy_route_doublestar(argm);
}

int main(int argc, char **argv) {
	std::cerr << std::endl << disclaimer << std::endl << std::endl;


/*	c_ip46_addr addr;
	std::cout << addr << std::endl;
	struct sockaddr_in sa;
	inet_pton(AF_INET, "192.0.2.33", &(sa.sin_addr));
	sa.sin_family = AF_INET;
	addr.set_ip4(sa);
	std::cout << addr << std::endl;

	struct sockaddr_in6 sa6;
	inet_pton(AF_INET6, "fc72:aa65:c5c2:4a2d:54e:7947:b671:e00c", &(sa6.sin6_addr));
	sa6.sin6_family = AF_INET6;
	addr.set_ip6(sa6);
	std::cout << addr << std::endl;
*/
	c_tunserver myserver;
	try {
		namespace po = boost::program_options;
		po::options_description desc("Options");
		desc.add_options()
			("help", "Print help messages")
			("devel","Test: used by developer to run current test")
			("develnum", po::value<int>()->default_value(1), "Test: used by developer to set current node number (makes sense with option --devel)")
			// ("K", po::value<int>()->required(), "number that sets your virtual IP address for now, 0-255")
			("myname", po::value<std::string>()->default_value("galaxy") , "a readable name of your node (e.g. for debug)")
			("mypub", po::value<std::string>()->default_value("") , "your public key (give any string, not yet used)")
			("mypriv", po::value<std::string>()->default_value(""), "your PRIVATE key (give any string, not yet used - of course this is just for tests)")
			//("peerip", po::value<std::vector<std::string>>()->required(), "IP over existing networking to connect to your peer")
			//("peerpub", po::value<std::vector<std::string>>()->multitoken(), "public key of your peer")
			("peer", po::value<std::vector<std::string>>()->multitoken(), "Adding entire peer reference, in syntax like ip-pub. Can be give more then once, for multiple peers.")
			;

		po::variables_map argm;
		try {
			po::store(po::parse_command_line(argc, argv, desc), argm);
			cout << "devel" << endl;
			if (argm.count("devel")) {
				try {
					bool should_continue = run_mode_developer(argm);
					if (!should_continue) return 0;
				}
				catch(std::exception& e) {
				    std::cerr << "Unhandled Exception reached the top of main: (in DEVELOPER MODE)" << e.what() << ", application will now exit" << std::endl;
						return 0; // no error for developer mode
				}
			}
			// argm now can contain options added/modified by developer mode
			po::notify(argm);


			if (argm.count("help")) {
				std::cout << desc;
				return 0;
			}

			_info("Configuring my own reference (keys):");
			myserver.configure_mykey_from_string(
				argm["mypub"].as<std::string>() ,
				argm["mypriv"].as<std::string>()
			);

			myserver.set_my_name( argm["myname"].as<string>() );

			_info("Configuring my peers references (keys):");
			vector<string> peers_cmdline;
			try { peers_cmdline = argm["peer"].as<vector<string>>(); } catch(...) { }
			for (const string & peer_ref : peers_cmdline) {
				myserver.add_peer_simplestring( peer_ref );
			}
		}
		catch(po::error& e) {
			std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
			std::cerr << desc << std::endl;
			return 1;
		}
	}
	catch(std::exception& e) {
		    std::cerr << "Unhandled Exception reached the top of main: "
				<< e.what() << ", application will now exit" << std::endl;
		return 2;
	}

	myserver.run();
}


