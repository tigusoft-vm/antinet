#ifndef include_crypto_hpp
#define include_crypto_hpp

#include "libs1.hpp"

#include "strings_utils.hpp"

using namespace std;

namespace antinet_crypto {

class c_symhash_state {
	public:

		typedef string_as_bin t_hash;

		c_symhash_state( t_hash initial_state );
		void next_state( t_hash additional_secret_material = t_hash("") );
		t_hash get_password() const;

		t_hash Hash1( const t_hash & hash ) const;
		t_hash Hash2( const t_hash & hash ) const;
	
	private:
		t_hash m_state;
		int m_number; ///< for debug mostly for now
};

c_symhash_state::c_symhash_state( t_hash initial_state )
	: m_state( initial_state ) 
{
	_info("Initial state: " << m_state.bytes);
	_info("Initial state dbg: " << string_as_dbg(m_state).get() );
}

void c_symhash_state::next_state( t_hash additional_secret_material ) {
	m_state = Hash1( Hash1( m_state ) + additional_secret_material );
	_info("State:" << m_state.bytes);
	++m_number;
}

c_symhash_state::t_hash c_symhash_state::get_password() const {
	return Hash2( m_state );
}


c_symhash_state::t_hash c_symhash_state::Hash1( const t_hash & hash ) const {
	return string_as_bin( "a(" + hash.bytes + ")" );
}

c_symhash_state::t_hash c_symhash_state::Hash2( const t_hash & hash ) const {
	return string_as_bin( "B(" + hash.bytes + ")" );
}






// ... 
// For given CA (form me, to given recipient) - and should be permanent

// ==================================================================

/***

DHDH CryptoAuth


On ID creation:
Ali: ap,AP = Generte_DH // make permanent keys: ap = Ali_DH_Perm_priv , AT = Ali_DH_Perm_Pub

On ID creation:
Bob: bp,BP = Generte_DH // same for Bob


On CA establishing from Alice to Bob:
Alice: 
  at,AT = Generte_DH()  // make temporary keys: at = Ali_DH_Tmp_priv , AT = Ali_DH_Tmp_Pub
  r1 = credentials_given_by_b || ';' || secure_random
  mt = "AT,r1"; // message with temporary key and temporary random

  KP = complete_DH( ap , BP ) , () // prepare permanent-key based AuthEncr
  uap=secure_random() ; ua = uap || encrypt_symm( KP , uap || mt )  // authenticate temp message
  deltaua = encrypt_symm ( HASHED_SYM_STATE.get_password() , uap )  // anti-QC!

Ali -> Bob: "deltaua" // NETWORK send

Bob: 
  decrypts/checks auth: ua 
  has AT, r1

  bt,BT = Generte_DH()  // make temporary keys: at = Ali_DH_Tmp_priv , AT = Ali_DH_Tmp_Pub
  r2 = secure_random() 
  mt = "BT,r2"; // message with temporary key and temporary random

  KP = complete_DH( bp , AP ) , // prepare permanent-key AuthEncr  
  ubp=secure_random() ; ub = ubp || encrypt_symm( KP , ubp || mt )  // authenticate temp message
  ... or ub = encrypt_symm( KP , ubp || ubp || mt )
  ... or ub = encrypt_symm( KP , 'all_is_fine' || mt )
  deltaub = encrypt_symm ( HASHED_SYM_STATE.get_password() , uab )  // anti-QC!

Bob -> Alice: "deltaub" // NETWORK send

Alice (and Bob similar)
  decrypts/checks auth: ub (and decrypts with HASHED_SYM_STATE)
  has BT, r2

Now both sides calculate the same:
  TSK = complete_DH( at , BT)  or  = complete_DH( bt , AT) // Temporary Shared Key
  FSSSK = H( H( TSK ) || H( r1 ) || H( r2 ) )   // Forward-Secrecy Session Shared Key
  nonce=1

*** r1,r2 help MAYBE(?) when:
attack hardness:   badluck-DH << DH-tmp , DH-perm 

*/


// For given CA (form me, to given recipient) - and given session
class c_dhdh_state {
	public:
		typedef string_as_bin t_pubkey;
		typedef string_as_bin t_privkey;
		typedef string_as_bin t_symkey; // symmetric key
		typedef long long int t_nonce;

		t_symkey m_skp; // shared key from permanent // KP = complete_DH( ap , BP ) , () // prepare permanent-key based AuthEncr

		c_dhdh_state(t_privkey our_priv, t_pubkey our_pub, t_pubkey theirs_pub);

		void step1();

	private: 
		t_privkey m_our_priv;
		t_pubkey m_our_pub;
		t_pubkey m_theirs_pub;
		t_symkey m_r; // my random r1 or r2 to use in CA shared key

		// const& ? TODO
		t_symkey execute_DH_exchange(t_privkey & my_priv , t_pubkey theirs_pub);

		t_symkey secure_random();

		t_nonce m_nonce;
};

c_dhdh_state::c_dhdh_state(t_privkey our_priv, t_pubkey our_pub, t_pubkey theirs_pub)
	: m_our_priv(our_priv), m_our_pub(our_pub), m_theirs_pub(theirs_pub)
{ }

void c_dhdh_state::step1() {
	m_r = secure_random();
	m_skp = execute_DH_exchange( m_our_priv , m_our_pub );
}

c_dhdh_state::t_symkey c_dhdh_state::execute_DH_exchange(t_privkey & my_priv , t_pubkey theirs_pub) {
	// TODO
	return t_symkey();
}

c_dhdh_state::t_symkey c_dhdh_state::secure_random() {
	return string_as_bin("RANDOM");
}


void test_crypto() {
		
	_mark("Testing crypto");

	#define SHOW _info( symhash.get_password().bytes );

	c_symhash_state symhash( string_as_hex("6a6b") ); // "jk"
	SHOW;
	symhash.next_state();
	SHOW;
	symhash.next_state();
	SHOW;
	symhash.next_state( string_as_bin("---RX-1---") );
	SHOW;
	symhash.next_state();
	SHOW;
	symhash.next_state( string_as_bin("---RX-2---") );
	SHOW;

	// SymHash




	// DH+DH


}

	

}


#endif

