#ifndef include_strings_utils_hpp
#define include_strings_utils_hpp

#include "libs0.hpp"


enum t_debug_style {
	e_debug_style_short_devel=1,
	e_debug_style_crypto_devel=2,
};

struct string_as_bin;

struct string_as_hex {
	std::string data; ///< e.g.: "1fab"

	string_as_hex()=default;
	string_as_hex(const std::string & s); ///< e.g. for "1fab" -> I will store "1fab"
	explicit string_as_hex(const string_as_bin & s); /// e.g. for "MN" I will store the hex representing bytes 0x4d(77) 0x4e(78) -> "4d4e"

	const std::string & get() const; ///< TODO(u) instead use operator<< to ostream so this is not usually needed... -- DONE?  just remove this probably
};
std::ostream& operator<<(std::ostream &ostr, const string_as_hex &obj);

bool operator==( const string_as_hex &a, const string_as_hex &b);

unsigned char hexchar2int(char c); // 'f' -> 15
unsigned char int2hexchar(unsigned char i); // 15 -> 'f'

unsigned char doublehexchar2int(string s); // "fd" -> 253


struct string_as_bin {
	std::string bytes;

	string_as_bin()=default;
	string_as_bin(const string_as_hex & encoded);
	string_as_bin(const char * ptr, size_t size); ///< build bytes from c-string data
	explicit string_as_bin(const std::string & bin); ///< create from an std::string, that we assume holds binary data

	/*template<class T>
	explicit string_as_bin( const T & obj  )
		: bytes( reinterpret_cast<const char*>( & obj ) , sizeof(obj) )
	{
		static_assert( std::is_pod<T>::value , "Can not serialize as binary data this non-POD object type");
	}*/

	template<class T, std::size_t N>
	explicit string_as_bin( const std::array<T,N> & obj )
		: bytes( reinterpret_cast<const char*>( obj.data() ) , sizeof(T) * obj.size() )
	{
		static_assert( std::is_pod<T>::value , "Can not serialize as binary data (this array type, of) this non-POD object type");
	}

	string_as_bin & operator+=( const string_as_bin & other );
	string_as_bin operator+( const string_as_bin & other ) const;
	string_as_bin & operator+=( const std::string & other );
	string_as_bin operator+( const std::string & other ) const;
	bool operator==(const string_as_bin &rhs) const;
	bool operator!=(const string_as_bin &rhs) const;
};

bool operator<( const string_as_bin &a, const string_as_bin &b);


std::string debug_simple_hash(const std::string & str);

std::string chardbg(char c); ///< Prints one character in "debugging" format, e.g. 0x0, or 0x20=32, etc.

struct string_as_dbg {
	public:
		std::string dbg; ///< this string is already nicelly formatted for debug output e.g. "(3){a,l,a,0x13}" or "(3){1,2,3}"

		string_as_dbg()=default;
		string_as_dbg(const string_as_bin & bin, t_debug_style style=e_debug_style_short_devel); ///< from our binary data string
		string_as_dbg(const char * data, size_t data_size, t_debug_style style=e_debug_style_short_devel); ///< from C style character bufer

		///! from range defined by two iterator-like objects
		template<class T>
		explicit string_as_dbg( T it_begin , T it_end, t_debug_style style=e_debug_style_short_devel )
		{
			std::ostringstream oss;
			oss << std::distance(it_begin, it_end) << ':';
			if (style==e_debug_style_crypto_devel) oss << "{hash=0x" << debug_simple_hash(std::string(it_begin, it_end)) << "}";
			oss<<'[';
			bool first=1;
			size_t size = it_end - it_begin;
			const size_t size1 = 8;
			const size_t size2 = 4;
			// TODO assert/review pointer operations
			if (size <= size1+size2) {
				for (auto it = it_begin ; it!=it_end ; ++it) { if (!first) oss << ','; print(oss,*it);  first=0;  }
			} else {
				{
					auto b = it_begin, e = std::min(it_end, it_begin+size1);
					for (auto it = b ; it!=e ; ++it) { if (!first) oss << ','; print(oss,*it);  first=0;  }
				}
				oss<<" ... ";
				first=1;
				{
					auto b = std::max(it_begin, it_end - size2), e = it_end;
					for (auto it = b ; it!=e ; ++it) { if (!first) oss << ','; print(oss,*it);  first=0;  }
				}
			}
			oss<<']';
			this->dbg = oss.str();
		}

		template<class T, std::size_t N> explicit string_as_dbg( const  typename std::array<T,N> & obj ) : string_as_dbg( obj.begin() , obj.end() ) { }

		operator const std::string & () const;
		const std::string & get() const;

	private:
		// print functions side affect: can modify flags of the stream os, e.g. setfill flag

		template<class T>	void print(std::ostream & os, const T & v) { os<<v; }


	public: // for chardbg.  TODO move to class & make friend class
		void print(std::ostream & os, unsigned char v, t_debug_style style=e_debug_style_short_devel );
		void print(std::ostream & os, signed char v, t_debug_style style=e_debug_style_short_devel );
		void print(std::ostream & os, char v, t_debug_style style=e_debug_style_short_devel );
};


// for debug mainly
template<class T, std::size_t N>
std::string to_string( const std::array<T,N> & obj ) {
	std::ostringstream oss;
	oss<<"("<<obj.size()<<")";
	oss<<'[';
	bool first=1;
	for(auto & v : obj) { if (!first) oss << ',';  oss<<v;  first=0;  }
	oss<<']';
	return oss.str();
}


std::string to_debug(const std::string & data, t_debug_style style=e_debug_style_short_devel);
std::string to_debug(const string_as_bin & data, t_debug_style style=e_debug_style_short_devel);
std::string to_debug(char data, t_debug_style style=e_debug_style_short_devel);

#endif


