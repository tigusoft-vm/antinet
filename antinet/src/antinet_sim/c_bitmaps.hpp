#ifndef C_BITMAPS_HPP
#define C_BITMAPS_HPP

#include "libs1.hpp"

/**
 * singleton
 */
class c_bitmaps {
public:
	static c_bitmaps &get_instance ();

	static void deinit ();

	BITMAP *m_background;
	BITMAP *m_node;
	BITMAP *m_package_green;
	BITMAP *m_package_blue;
	BITMAP *m_package_red;
	BITMAP *m_package_white;
    BITMAP *m_bitmap_font1;

    int m_background_opengl;
    int m_node_opengl;
    int m_pack_green_opengl;
    int m_pack_blue_opengl;
    int m_pack_red_opengl;
    int m_pack_white_opengl;
    int m_bitmap_font1_opengl;

	~c_bitmaps ();

private:
	c_bitmaps ();


	c_bitmaps (const c_bitmaps &) = delete;

	c_bitmaps (c_bitmaps &&) = delete;

	void operator= (c_bitmaps &&) = delete;

	void operator= (c_bitmaps const &) = delete;

	static std::unique_ptr<c_bitmaps> m_instance;
	static std::once_flag m_onceFlag;

	void init (); ///< Must be called (e.g. from singleton) before first use.
	void init_find_path (); ///< Get path to the data dir
	void init_load_all (); ///< Load all data (that we say should be loaded on Init)
    void bitmap_to_int (); ///convert bitmap to int - it is necessery to use opengl texture

	bool test_dir_as_datadir (string dir) const; ///< return is this the data dir
	string m_path_to_data; ///< The path (can be: relative, absolute, with prefix) (must end with "/") to reach data dir
};


#endif // include guard

