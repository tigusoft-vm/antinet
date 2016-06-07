#include "generate_config.hpp"

std::string generate_config::m_crypto_set_name = "";


void generate_config::crypto_set(e_crypto_set cryptoset) {

	antinet_crypto::c_multikeys_PAIR keypair;

	switch(cryptoset) {
	case e_crypto_set::highest:
		keypair.generate(antinet_crypto::e_crypto_system_type_X25519, 2);
		keypair.generate(antinet_crypto::e_crypto_system_type_SIDH, 2);
		keypair.generate(antinet_crypto::e_crypto_system_type_NTRU_EES439EP1, 2);
		break;
	case e_crypto_set::high:
		keypair.generate(antinet_crypto::e_crypto_system_type_X25519, 1);
		keypair.generate(antinet_crypto::e_crypto_system_type_SIDH, 1);
		keypair.generate(antinet_crypto::e_crypto_system_type_NTRU_EES439EP1, 1);
		break;
	case e_crypto_set::normal:
		keypair.generate(antinet_crypto::e_crypto_system_type_X25519, 1);
		keypair.generate(antinet_crypto::e_crypto_system_type_NTRU_EES439EP1, 1);
		break;
	case e_crypto_set::fast:
		keypair.generate(antinet_crypto::e_crypto_system_type_X25519, 1);
		keypair.generate(antinet_crypto::e_crypto_system_type_NTRU_EES439EP1, 1);
		break;
	case e_crypto_set::lowest:
		keypair.generate(antinet_crypto::e_crypto_system_type_X25519, 1);
		break;
	case e_crypto_set::idp_normal:
		keypair.generate(antinet_crypto::e_crypto_system_type_Ed25519, 1);
		keypair.generate(antinet_crypto::e_crypto_system_type_NTRU_sign, 1);
		break;
	}
	keypair.datastore_save_PRV_and_pub(generate_config::m_crypto_set_name);
}
