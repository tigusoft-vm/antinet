#include "c_bitmaps.hpp"

#include "c_allegromisc.hpp"
#include "use_opengl.hpp"

std::unique_ptr<c_bitmaps> c_bitmaps::m_instance;
std::once_flag c_bitmaps::m_onceFlag;

c_bitmaps &c_bitmaps::get_instance () {
	std::call_once(m_onceFlag, [] {
		m_instance.reset(new c_bitmaps);
		m_instance->init();
	});
	return *m_instance.get();
}

void c_bitmaps::deinit () {
	m_instance.reset(nullptr);
}


c_bitmaps::c_bitmaps ()
:
	m_background(nullptr),
	m_node(nullptr),
	m_package_green(nullptr),
	m_package_blue(nullptr),
	m_package_red(nullptr),
	m_package_white(nullptr),
	m_bitmap_font1(nullptr),
	m_background_opengl(),
	m_node_opengl(),
	m_pack_green_opengl(),
	m_pack_blue_opengl(),
	m_pack_red_opengl(),
	m_pack_white_opengl(),
	m_bitmap_font1_opengl(),
	m_path_to_data()
{
}

c_bitmaps::~c_bitmaps () {
	destroy_bitmap(m_background);
	destroy_bitmap(m_node);
	destroy_bitmap(m_package_green);
	destroy_bitmap(m_package_blue);
	destroy_bitmap(m_package_red);
	destroy_bitmap(m_package_white);
    destroy_bitmap(m_bitmap_font1);
}


void c_bitmaps::init () {
	init_find_path();
	init_load_all();
    bitmap_to_int();
}

void c_bitmaps::init_find_path () {
	const vector<string> dirs = {"./data/", "../data/", "../../data/", "./src/data/"}; // possible path to read the dir
	for (const string &dir : dirs) {
		if (test_dir_as_datadir(dir)) {
			m_path_to_data = dir;
			break;
		}
	}
	if (m_path_to_data.empty())
		throw std::runtime_error("could not find path to data");

	std::cout << "m_path_to_data=" << m_path_to_data << std::endl;
}

void c_bitmaps::init_load_all () {
    m_background = alex_load_png(m_path_to_data + "bgr-bright.png", NULL); /// "bgr-bright.png"
    m_node = alex_load_png(m_path_to_data + "server_48x48.png", NULL);
	m_package_green = alex_load_png(m_path_to_data + "letter_21x11_green.png", NULL);
	m_package_blue = alex_load_png(m_path_to_data + "letter_21x11_blue.png", NULL);
	m_package_red = alex_load_png(m_path_to_data + "letter_21x11_red.png", NULL);
    m_package_white = alex_load_png(m_path_to_data + "letter_21x11.png", NULL);
    m_bitmap_font1 = alex_load_png(m_path_to_data + "font1.png", NULL);
}

void c_bitmaps::bitmap_to_int() {
    m_background_opengl = allegro_gl_make_texture(m_background);
    m_node_opengl = allegro_gl_make_texture_ex(AGL_TEXTURE_HAS_ALPHA, m_node, -1);
    m_pack_green_opengl = allegro_gl_make_texture_ex(AGL_TEXTURE_HAS_ALPHA, m_package_green, -1);
    m_pack_blue_opengl = allegro_gl_make_texture_ex(AGL_TEXTURE_HAS_ALPHA, m_package_blue, -1);
    m_pack_red_opengl = allegro_gl_make_texture_ex(AGL_TEXTURE_HAS_ALPHA, m_package_red, -1);
    m_pack_white_opengl = allegro_gl_make_texture_ex(AGL_TEXTURE_HAS_ALPHA, m_package_white, -1);
    m_bitmap_font1_opengl = allegro_gl_make_texture(m_bitmap_font1);
}

bool c_bitmaps::test_dir_as_datadir (string dir) const {
	std::ifstream testfile(dir + "dir_data.txt");
	bool ok = testfile.good() && (!testfile.eof());
	return ok;
}
