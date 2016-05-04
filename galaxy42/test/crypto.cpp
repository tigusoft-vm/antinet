#include "gtest/gtest.h"
#include "../crypto.hpp"

namespace antinet_crypto {

TEST(crypto, aeshash_not_repeating_state_nor_password) {
	std::set< c_symhash_state::t_hash > used_hash;
	const int amount_iterations = 10000;

    enum class type_RX : int { RX_none=0, RX_constant, RX_same, RX_random , RX_END };


    for (auto rx_type = type_RX::RX_none ; rx_type < type_RX::RX_END ; rx_type = static_cast<type_RX>(static_cast<int>(rx_type) + 1)  ) {
		c_symhash_state symhash( "" );

		auto rx_same = symhash.secure_random( 10 );

		switch (rx_type) { // first nextstate after creation of symhash
            case type_RX::RX_none:			break; // here we do not do it, but must be then done in others to avoid collision
            case type_RX::RX_constant:		symhash.next_state( "foo" );
                                            break;
            case type_RX::RX_same:			symhash.next_state( rx_same );
                                            break;
            case type_RX::RX_random:		symhash.next_state( symhash.secure_random( 2 ) );
                                            break;
            default: assert(false);
		}


		for (int i=0; i<amount_iterations; ++i) {
			{
				auto result = used_hash.insert( symhash.get_the_SECRET_PRIVATE_state() );
				ASSERT_TRUE( result.second == true ); // inserted new one
			}
			{
				auto result = used_hash.insert( symhash.get_password() );
				ASSERT_TRUE( result.second == true ); // inserted new one
			}
			switch (rx_type) {
                case type_RX::RX_none:		symhash.next_state();
                                            break;
                case type_RX::RX_constant:	symhash.next_state( "foo" );
                                            break;
                case type_RX::RX_same:		symhash.next_state( rx_same );
                                            break;
                case type_RX::RX_random:	symhash.next_state( symhash.secure_random( 2 ) );
                                            break;
				default: assert(false);
			}
		} // all iterations of using it


		switch (rx_type) {
            case type_RX::RX_none:
				EXPECT_EQ( string_as_hex(symhash.get_password()) , string_as_hex("084b0ff5a81c8f3c1001b2d596cc02db629ea047716eba8440bb823223f18bddaa3631a02e43bbf886584cc636eb0a56a5813f15c9c0aeb3b5b4b4877221da8e") );
				EXPECT_EQ( string_as_hex(symhash.get_the_SECRET_PRIVATE_state()) , string_as_hex("b64bde9d13b26847df387b6aa2f475a1309b64d14aaa07877df1b43cd1c79364ff7d7fbbef222ec41d55bb21f4144124c91d69a7411d3a4e29178a7e6748e097") );
			break;
			default: break;
		}
	}
}


TEST(crypto, aeshash_start_and_get_same_passwords) {
	c_symhash_state symhash( string_as_bin(string_as_hex("6a6b")).bytes ); // "jk"
	auto p = string_as_hex( string_as_bin( symhash.get_password() ) );
	//	cout << "\"" << string_as_hex( symhash.get_password() ) << "\"" << endl;
	EXPECT_EQ(p.get(), "1ddb0a828c4d3776bf12abbe17fb4d82bcaf202a1b00b5b54e90db701303d69ce235f36d25c9fd1343225888e00abdc0e18c2036e86af9f3a90faf1abfefedf7");
	symhash.next_state();

	p = string_as_hex( symhash.get_password() );
	EXPECT_EQ(p.get() ,"72e4af0f04e2113852fd0d5320a14aeb2219d93ed710bc9bd72173b4ca657f37e4270c8480beb8fded05b6161d32a6450d4c3abb86023984f4f9017c309b5330");

	symhash.next_state();
	p = string_as_hex( symhash.get_password() );
	EXPECT_EQ(p.get(), "8a986c419f1347d8ea94b3ad4b9614d840bb2dad2e13287a7a6cb5cf72232c3211997b6435f44256a010654d6f49e71517e46ce420a77f09f3a425eabaa99d8a");
}

TEST(crypto, dh_exchange) {
	auto alice_keys = c_dhdh_state::generate_key_pair();
	auto alice_pub = alice_keys.first;
	auto alice_priv = alice_keys.second;
	EXPECT_EQ(alice_pub, alice_keys.first);
	EXPECT_EQ(alice_priv, alice_keys.second);

	auto bob_keys = c_dhdh_state::generate_key_pair();
	auto bob_pub = bob_keys.first;
	auto bob_priv = bob_keys.second;
	EXPECT_EQ(bob_pub, bob_keys.first);
	EXPECT_EQ(bob_priv, bob_keys.second);

	EXPECT_NE(alice_pub, bob_pub);
	EXPECT_NE(alice_priv, bob_priv);

	EXPECT_NE(alice_pub, alice_priv);
	EXPECT_NE(bob_pub, bob_priv);

	c_dhdh_state alice_state(alice_priv, alice_pub, bob_pub);
	c_dhdh_state bob_state(bob_priv, bob_pub, alice_pub);

	auto alice_sym_key = alice_state.execute_DH_exchange();
	auto bob_sym_key = bob_state.execute_DH_exchange();
	EXPECT_EQ(alice_sym_key, bob_sym_key);
}

} // namespace
