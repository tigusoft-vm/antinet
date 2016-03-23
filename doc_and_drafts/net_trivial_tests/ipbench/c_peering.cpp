#include "c_peering.hpp"

#include <sodium.h>

#include "protocol.hpp"

c_peering::c_peering(const c_ip46_addr & addr_peering , const c_haship_pubkey & pubkey , const c_haship_addr & haship_addr)
 : m_addr(addr_peering), m_pubkey(pubkey), m_haship_addr(haship_addr)
{
	_info("I am new peer, with wire addr="<<addr_peering<<" and pubkey="<<to_string(pubkey)<<" and haship_addr="<<to_string(haship_addr));
}


// ------------------------------------------------------------------

c_peering_udp::c_peering_udp(const c_ip46_addr & addr_peering , const c_haship_pubkey & pubkey , const c_haship_addr & haship_addr)
	: c_peering(addr_peering, pubkey, haship_addr)
{ }

void c_peering_udp::send_data(const char * data, size_t data_size) {
	throw std::runtime_error("Use send_data_udp");
}


// TODO unify array types! string_as_bin , unique_ptr to new c-array, raw c-array in libproto etc

void c_peering_udp::send_data_udp(const char * data, size_t data_size, int udp_socket) {
	_info("Send to peer (tunneled data) data: " << string_as_dbg(data,data_size).get() ); // TODO .get

	static unsigned char generated_shared_key[crypto_generichash_BYTES] = {43, 124, 179, 100, 186, 41, 101, 94, 81, 131, 17,
					198, 11, 53, 71, 210, 232, 187, 135, 116, 6, 195, 175,
					233, 194, 218, 13, 180, 63, 64, 3, 11};

	static unsigned char nonce[crypto_aead_chacha20poly1305_NPUBBYTES] = {148, 231, 240, 47, 172, 96, 246, 79};
	static unsigned char additional_data[] = {1, 2, 3};
	static unsigned long long additional_data_len = 3; // TODO

	// TODO randomize this data XXX use real crypto data

	const int header_size = c_protocol::version_size + c_protocol::cmd_size ; // TODO pack this into protocol class --r

	// TODO allocate buffer outside
	std::unique_ptr<unsigned char[]> protomsg(new unsigned char[data_size + crypto_aead_chacha20poly1305_ABYTES + header_size ]); // plus the headers
	// long long? or u64? --r assert size  sizeof(long long  .... == ...

	// why? --r:
	assert(crypto_aead_chacha20poly1305_KEYBYTES <= crypto_generichash_BYTES);
	protomsg.get()[0] = c_protocol::current_version;
	protomsg.get()[1] = c_protocol::e_proto_cmd_tunneled_data;

	unsigned char * ciphertext_buf = protomsg.get() + header_size; // just-pointer to part of protomsg where to write the message!
	unsigned long long ciphertext_buf_len = 0; // encryption will write here the resulting size
	crypto_aead_chacha20poly1305_encrypt(ciphertext_buf, &ciphertext_buf_len, (unsigned char *)data, data_size, additional_data,
	                                     additional_data_len, NULL, nonce, generated_shared_key);
	unsigned long long protomsg_len = ciphertext_buf_len + header_size; // the output of crypto, plus the header in front

	// TODO asserts!!!

	this->send_data_RAW_udp( reinterpret_cast<const char *>(protomsg.get()), protomsg_len, udp_socket); // reinterpret: char/unsigned char
}

void c_peering_udp::send_data_udp_cmd(c_protocol::t_proto_cmd cmd, const string_as_bin & bin, int udp_socket) {
	_info("Send to peer (COMMAND): command="<<static_cast<int>(cmd)<<" data: " << string_as_dbg(bin).get() ); // TODO .get
	string_as_bin raw;
	raw.bytes += (unsigned char) c_protocol::current_version;
	raw.bytes += (unsigned char) cmd;
	raw.bytes += bin.bytes;
	this->send_data_RAW_udp(raw.bytes.c_str(), raw.bytes.size(), udp_socket);
}

void c_peering_udp::send_data_RAW_udp(const char * data, size_t data_size, int udp_socket) {
	_info("UDP send to peer RAW: " << string_as_dbg(data,data_size).get() ); // TODO .get

	switch (m_addr.get_ip_type()) {
		case c_ip46_addr::t_tag::tag_ipv4 : {
			auto ip_x = m_addr.get_ip4(); // ip of proper type, as local variable
			sendto(udp_socket, data, data_size, 0, reinterpret_cast<sockaddr*>( & ip_x ) , sizeof(sockaddr_in) );
		}
		break;
		case c_ip46_addr::t_tag::tag_ipv6 : {
			auto ip_x = m_addr.get_ip6(); // ip of proper type, as local variable
			sendto(udp_socket, data, data_size, 0, reinterpret_cast<sockaddr*>( & ip_x ) , sizeof(sockaddr_in6) );
		}
		break;
		default: {
			std::ostringstream oss; oss << m_addr; // TODO
			throw std::runtime_error(string("Invalid IP type: ") + oss.str());
		}
	}
}

