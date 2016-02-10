#ifndef C_MINT_H
#define C_MINT_H

#include <chrono>
#include "c_token.hpp"
#include "../../crypto_ops/crypto/c_random_generator.hpp"
#include <map>
#include <string>

using std::string;

class token_id_generator {
  public:
    token_id_generator();
    size_t generate_id();
  private:
    size_t id;
};

class c_mint {
  public:
    c_mint(std::string mintname, std::string pubkey, std::chrono::seconds exp_time = std::chrono::hours(72));
    std::string m_mintname;
    std::string m_pubkey;

    c_token emit_token();

    bool check_isEmited(c_token &);
    size_t clean_expired_tokens();
    void print_mint_status(std::ostream &os) const;
    long get_last_expired_id() const;
  private:
	std::map<c_token, long long> m_emited_tokens;
    c_random_generator<long long> random_generator;

    long m_last_expired_id;

    /// expitation_time of token
    /// all token emited by this mint should have the same expiration time
    /// This helps in the subsequent token verification
    std::chrono::seconds t_expiration_time;
    long long generate_password();
    token_id_generator m_id_generator;
};

#endif // C_MINT_H
