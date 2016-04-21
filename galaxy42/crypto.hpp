#ifndef include_crypto_hpp
#define include_crypto_hpp

#include "libs1.hpp"
#include <sodium.h>
#include "strings_utils.hpp"
#include "gtest/gtest_prod.h"

using namespace std;

namespace antinet_crypto {

namespace unittest {
	class c_symhash_state__tests_with_private_access;
} // namespace


enum e_crypto_system_type : int {
	t_crypto_system_type_ = 5,
	t_ = 6,
};


/***
 * Some state of some crypto system
*/
class c_crypto_state {
	public:
		typedef string_as_bin t_symkey; // symmetric key

		virtual ~c_crypto_state()=default;

		virtual t_symkey secure_random(size_t size_of_radom_data) const;
};



class c_symhash_state final : public c_crypto_state {
	public:
		typedef string_as_bin t_hash;

		c_symhash_state( t_hash initial_state );
		void next_state( t_hash additional_secret_material = t_hash("") );
		t_hash get_password() const;

		t_hash Hash1( const t_hash & hash ) const;
		t_hash Hash2( const t_hash & hash ) const;

	private:
		t_hash get_the_SECRET_PRIVATE_state() const; //< used e.g. by some tests
		friend class unittest::c_symhash_state__tests_with_private_access;
		FRIEND_TEST(crypto, aeshash_not_repeating_state_nor_password);

	
	private:
		t_hash m_state;
		int m_number; ///< for debug mostly for now
};



// For given CA (form me, to given recipient) - and given session
class c_dhdh_state final : public c_crypto_state {
	public:
		typedef string_as_bin t_pubkey;
		typedef string_as_bin t_privkey;
		typedef long long int t_nonce;

		t_symkey m_skp; // shared key from permanent // KP = complete_DH( ap , BP ) , () // prepare permanent-key based AuthEncr

		c_dhdh_state(t_privkey our_priv, t_pubkey our_pub, t_pubkey theirs_pub);

		void step1();
		t_pubkey get_permanent_pubkey() const;
		t_pubkey get_temp_pubkey() const;
		static std::pair<t_pubkey, t_privkey> generate_key_pair();

	private:
		t_privkey m_our_priv; ///< permanent
		t_pubkey m_our_pub; ///< permanent
		t_pubkey m_theirs_pub;
		t_symkey m_r; // my random r1 or r2 to use in CA shared key

		t_pubkey m_pubkey_temp;
		t_privkey m_privkey_temp;

		t_symkey execute_DH_exchange(const t_privkey &my_priv, const t_pubkey &my_pub, const t_pubkey &theirs_pub);
		t_symkey execute_DH_exchange(); ///< call execute_DH_exchange with internal fields

		FRIEND_TEST(crypto, dh_exchange);

		t_nonce m_nonce;
};




void test_crypto();
	

} // namespace


#endif

