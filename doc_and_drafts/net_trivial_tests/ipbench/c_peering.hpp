#ifndef C_PEERING_H
#define C_PEERING_H

#include "c_ip46_addr.hpp"
#include "haship.hpp"
#include "protocol.hpp"

// TODO (later) make normal virtual functions (move UDP properties into class etc) once tests are done.

struct t_peering_reference {
	public:
		t_peering_reference(const string &peering_addr, const string_as_hex &peering_pubkey); // input data from strings e.g. from config text
		t_peering_reference(const string &peering_addr, const string_as_bin &peering_pubkey); // input data from binary data

	public:
		c_haship_pubkey pubkey;
		c_haship_addr haship_addr;
		c_ip46_addr peering_addr;
};

// TODO: crypto options here
class c_peering { ///< An (mostly established) connection to peer
	public:
		c_peering(const c_ip46_addr & addr_peering , const c_haship_pubkey & pubkey , const c_haship_addr &);

		virtual void send_data(const char * data, size_t data_size)=0;
		virtual ~c_peering()=default;

	protected:
		c_ip46_addr	m_addr; ///< peer address in socket format

		c_haship_pubkey m_pubkey; ///< his pubkey
		c_haship_addr m_haship_addr; ///< his haship address
		// ... TODO crypto type
};

class c_peering_udp : public c_peering { ///< An established connection to UDP peer
	public:
		c_peering_udp(const c_ip46_addr & addr_peering , const c_haship_pubkey & pubkey , const c_haship_addr &);

		virtual void send_data(const char * data, size_t data_size);
		virtual void send_data_udp(const char * data, size_t data_size, int udp_socket);
		virtual void send_data_udp_cmd(c_protocol::t_proto_cmd cmd, const string_as_bin & bin, int udp_socket);
	private:

		virtual void send_data_RAW_udp(const char * data, size_t data_size, int udp_socket); ///< direct write
};

#endif // C_PEERING_H
