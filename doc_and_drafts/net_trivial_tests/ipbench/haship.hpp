
#include "strings_utils.hpp"
#include "libs1.hpp"

// declare sizes; also forward declarations
constexpr int g_haship_addr_size = 16;
constexpr int g_haship_pubkey_size = 32;
struct c_haship_addr;
struct c_haship_pubkey;

/***
@class virtual hash-ip, e.g. ipv6, usable for ipv6-cjdns (fc00/8), and of course also for our ipv6-galaxy (fd42/16)
*/
struct c_haship_addr : public std::array<unsigned char, g_haship_addr_size> {
	struct tag_constr_by_hash_of_pubkey{};
	struct tag_constr_by_addr_string{};

	c_haship_addr();
	c_haship_addr(tag_constr_by_hash_of_pubkey x, const c_haship_pubkey & pubkey ); ///< create the IP address that matches given public key (e.g. hash of it)
	c_haship_addr(tag_constr_by_addr_string x, const string & pubkey ); ///< create the IP address from a string (as dot/colon IP notation)

	void print(ostream &ostr) const;
};
ostream& operator<<(ostream &ostr, const c_haship_addr & v);


struct c_haship_pubkey : std::array<unsigned char, g_haship_pubkey_size > {
	c_haship_pubkey();
	c_haship_pubkey( const string_as_bin & input ); ///< create the IP form
};

