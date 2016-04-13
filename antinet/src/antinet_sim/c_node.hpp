#ifndef C_NODE_HPP
#define C_NODE_HPP

#include "libs1.hpp"
#include "osi2.hpp"
#include "c_osi3_uuid_generator.hpp"
#include "c_world.hpp"
#include "c_msg.hpp"
#include "c_gui.hpp"
#include "c_dijkstry.hpp"
#include "c_geometry.hpp"

class c_osi2_switch;
class c_osi2_cable_direct; 
class c_osi2_nic;
class c_world;

// Classical switch in OSI layer 2
class c_osi2_switch : public c_entity {
	protected:
		static long int s_nr; ///< serial number of this object - the static counter
		long int m_nr; ///< serial number of this object

		friend class c_world;

		c_world &m_world; ///< my netwok world in which I exist
		
		std::vector<std::unique_ptr<c_osi2_nic>> m_nic; ///< all my NIC cards, for all my ports
		std::queue<std::pair<t_osi3_uuid,double>> m_draw_outbox; ///< vector of packet destination to draw with drew part.
		
		std::vector<t_osi3_packet> m_outbox; ///< data that I will wish to send (over some NIC)
		std::vector<t_osi3_packet> m_inbox;
		
		const unsigned int m_connect_cost = 1; ///< TODO delete?
		
		t_device_type m_type;

	public:

		void send_package(t_osi3_packet &&);
		c_osi2_switch(c_world &world, const string &name, t_pos x, t_pos y);
		
		c_osi2_switch(const c_osi2_switch &) = delete; ///< copy constructor
		c_osi2_switch& operator = (const c_osi2_switch &)  = delete;
		c_osi2_switch(c_osi2_switch &&) = default; ///< move constructor
		c_osi2_switch& operator = (c_osi2_switch &&)  = default;
		~c_osi2_switch() = default;

		bool operator == (const c_osi2_switch &switch_);
		bool operator != (const c_osi2_switch &switch_);
		
		// work on my NICs:
		// work on my NICs:		
		void create_nic(); ///< adds one more NIC card
		c_osi2_nic & get_nic(unsigned int nr); ///< gets NIC with this number, throws if it does not exist
		c_osi2_nic & use_nic(unsigned int nr); ///< gets NIC with this number, can create it (and all other up to that number)
		
		t_device_type get_type() override;
		
		/**
		 * @brief returns the last valid index in our m_nic card list
		 * @return the index, or invalid size_t
		 */
		size_t_maybe get_last_nic_index() const; ///< gets number of last NIC
		
		/**
		 * @brief return index in m_nic[] of the card tha connects to given switch
		 * @param obj must be the switch living in my world m_object, because we compare directly by mem-address
		 * @return the index, or invalid size_t
		 */
		size_t_maybe find_which_nic_goes_to_switch_or_invalid(const c_osi2_switch * obj);
		
		t_osi3_uuid get_uuid_any(); ///< get some kind of UUID address that is mine (e.g. from first NIC, or make one, etc)
		
		void connect_with(c_osi2_nic &target, c_world &world, t_osi2_cost cost); ///< add port, connect to target, inside world
		
		unsigned int get_cost(); ///< TODO delete?
		
		virtual void print(std::ostream & oss) const override;
		virtual void print(std::ostream &os, int level=0) const;
		std::string print_str(int level=0) const;
		friend std::ostream& operator<<(std::ostream &os, const c_osi2_switch &obj);

		c_world & get_world() const;
		
		// === tick ===
		virtual void logic_tick() override; ///< move packets from inbox to outbox
		virtual void recv_tick() override;
		virtual void send_tick() override;
		
		virtual void draw_allegro(c_drawtarget &drawtarget, c_layer &layer_any) override;
		virtual void draw_messages(c_drawtarget &drawtarget, c_layer &layer_any) const;
		virtual void draw_packet(c_drawtarget &drawtarget, c_layer &layer_any);
		// TODO mv to node
		void send_hello_to_neighbors(); ///< send HELLO packet to all neighbors
};

class c_node : public c_osi2_switch {
	private:	
		// TODO add s_nr m_nr print and operator<< like in others
		
	public:
		c_node(c_world &world, const string &name, t_pos x, t_pos y);
		c_node(const c_node &) = delete; ///< copy constructor
		c_node& operator = (const c_node &)  = delete;
		c_node(c_node &&) = default; ///< move constructor
		c_node& operator = (c_node &&)  = default;
		~c_node() = default;
		
		bool operator == (const c_node &node);
		bool operator != (const c_node &node);
		
		// === data sending ===
		/***
		 * @brief Send network data to given UUID (IP) destination, anywhere in same world
		 */
		void send_osi3_data_to_dst(t_osi3_uuid dst, t_osi2_data &&data);
		
		/***
		 * @brief Send network data to given UUID (IP) destination, anywhere in same world
		 * the target is given by name, and right now a routing decision is made enough
		 * to decide to which of the "IP addresses" / to which NIC card should we send the data
		 * (to that one that we can find route to)
		 * This is e.g. for easier testing
		 * @param c_node - that is the object (that lives in our world's m_object), as reference, to which we will send
		 * @param data is the payload data
		 */
		void send_osi3_data_to_node(const c_node & dst, std::string &&data);
		
		/**
		 * @brief same, but the target is given by name 
		 * @see send_osi3_data_to_node();
		 * @param c_node - that is the object, given by name, (that lives in our world's m_object) to which we will send
		 * @param data is the payload data
		 */
		void send_osi3_data_to_name(const std::string &dest_name, std::string &&data);


		virtual void draw_allegro(c_drawtarget &drawtarget, c_layer &layer_any) override;

		// === tests ===
		void send_hello_to_neighbors(); ///< send HELLO packet to all neighbors

		// === tick ===
		virtual void logic_tick() override;

		void process_packet(t_osi3_packet &&packet);
		//		virtual void logic_tick() override;
};

#endif // C_NODE_HPP
