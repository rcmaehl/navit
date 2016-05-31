/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2016 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>

#include <math.h>

#include <glib.h>
#include <time.h>


#include <string.h>
#include <errno.h>

#include "config.h"
#include "config_.h"
#include "navit.h"
#include "coord.h"
#include "point.h"
#include "plugin.h"
#include "debug.h"
#include "item.h"
#include "xmlconfig.h"
#include "attr.h"
#include "layout.h"
#include "navigation.h"
#include "command.h"
#include "callback.h"
#include "graphics.h"
#include "track.h"
#include "vehicle.h"
#include "vehicleprofile.h"
#include "map.h"
#include "event.h"
#include "mapset.h"
#include "osd.h"
#include "route.h"
#include "search.h"
#include "callback.h"
#include "gui.h"
#include "util.h"



#define CRC_POLYNOME 0xAB
char x[] = {0x66, 0x55,0x44, 0x33, 0x22, 0x11};
char result[6] = {0,};
unsigned char i2ctxdata[128] = {0,};
unsigned char i2crxdata[128] = {0,};


typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef signed char int8_t;
typedef signed short int int16_t;

struct i2c{
	struct navit *nav;
    unsigned char addr[10];
    int device;
    struct callback* callback;
    int timeout;
};

typedef struct txdataLSG{
	bool AL;
	bool TFL;
	bool ZV;
	bool LED;
	uint8_t time_in;
	uint8_t time_out;
}tx_lsg_t;
	
typedef struct rxdataLSG{
	bool AL;
	bool TFL;
	bool ZV;
	bool LED;
	uint8_t time_in;
	uint8_t time_out;
}rx_lsg_t;

typedef struct txdataV2V{
	uint8_t time;
	uint8_t temperature;
}tx_v2v_t;
	
typedef struct rxdataV2V{
	uint8_t time;
	uint8_t temperature;
}rx_v2v_t;

typedef struct txdataWFS{
	uint8_t time;
}tx_wfs_t;
	
typedef struct rxdataWFS{
	uint8_t time;
}rx_wfs_t;

typedef struct txdataMFA{
	uint8_t radio_text[32];
	uint8_t navigation_next_turn;
	long distance_to_next_turn;
	//navigation active?
	uint8_t cal_water_temperature;
	uint8_t cal_voltage;
	uint8_t cal_oil_temperature;
	uint8_t cal_consumption;	
	// other values from pwm module?
}tx_mfa_t;
	
typedef struct rxdataMFA{
	uint8_t radio_text[32];
	uint8_t navigation_next_turn;
	long distance_to_next_turn;
	//navigation active?
	uint8_t cal_water_temperature;
	uint8_t cal_voltage;
	uint8_t cal_oil_temperature;
	uint8_t cal_consumption;
	// read only
	uint16_t voltage;
	int8_t water_temperature;
	int8_t ambient_temperature;
	int8_t oil_temperature;
	int consumption;
	int average_consumption;
	int range;
	int speed;
	int average_speed;
	int rpm;
}rx_mfa_t;

typedef struct txdataPWM{
	uint16_t pwm_freq;
	uint8_t cal_temperature;
	uint8_t cal_voltage;
	uint8_t water_value;
	uint8_t time_value;
}tx_pwm_t;
	
typedef struct rxdataPWM{
	uint16_t pwm_freq;
	uint8_t cal_temperature;
	uint8_t cal_voltage;
	uint8_t water_value;
	uint8_t time_value;
	uint16_t vbat;
	uint8_t water_temp;
	uint8_t fet_temp;
}rx_pwm_t;


rx_lsg_t *rx_lsg;
tx_lsg_t *tx_lsg;
rx_pwm_t *rx_pwm;
tx_pwm_t *tx_pwm;
rx_wfs_t *rx_wfs;
tx_wfs_t *tx_wfs;
rx_v2v_t *rx_v2v;
tx_v2v_t *tx_v2v;
rx_mfa_t *rx_mfa;
tx_mfa_t *tx_mfa;


void read_i2c_frame(int device, uint8_t* data, uint8_t size);

uint8_t calculateID(char* name){
	//calculate an ID from the first 3 Letter of its name
	uint8_t ID;
	ID = (name[0]-48) * 3 + (name[1]-48) * 2 + (name[2]-48);
	//ID = (ID >> 1);
	dbg(lvl_debug,"Name: %s, ID = 0x%02X\n",name, ID);
	return ID;
}

void init_i2c_data(void){
	rx_lsg = (rx_lsg_t*) malloc(sizeof(rx_lsg_t)); 
	tx_lsg = (tx_lsg_t*) malloc(sizeof(tx_lsg_t)); 
	rx_pwm = (rx_pwm_t*) malloc(sizeof(rx_pwm_t)); 
	tx_pwm = (tx_pwm_t*) malloc(sizeof(tx_pwm_t)); 
	rx_wfs = (rx_wfs_t*) malloc(sizeof(rx_wfs_t)); 
	tx_wfs = (tx_wfs_t*) malloc(sizeof(tx_wfs_t)); 
	rx_v2v = (rx_v2v_t*) malloc(sizeof(rx_v2v_t)); 
	tx_v2v = (tx_v2v_t*) malloc(sizeof(tx_v2v_t)); 
	rx_mfa = (rx_mfa_t*) malloc(sizeof(rx_mfa_t)); 
	tx_mfa = (tx_mfa_t*) malloc(sizeof(tx_mfa_t)); 
}

uint8_t crc8(uint8_t crc, uint8_t data){
	uint8_t i, _data = 0;
	_data = crc ^ data;
	for(i=0; i<8; i++){
		if((_data & 0x80) != 0){
			_data <<= 1;
			_data ^= 0x07;
		}else{
			_data <<= 1;
		}
	}
	printf(" 0x%02X",_data);
	return _data;
}

uint8_t calculateCRC8(uint8_t crc, uint8_t* data, uint8_t len){
	dbg(lvl_debug,"crc: %p, %i\n",data, crc);
	while(len-->0){
		//dbg(lvl_debug,"crc: %p, %i\n",data, crc);
		crc = crc8(crc, *data++);
	}
	dbg(lvl_debug,"\ncrc: %i\n",data, crc);
	return crc;
}


int open_i2c(const char* device){
	int dev_h = open(device, O_RDWR);
	if(dev_h < 0){
		perror("Error: Can't open I2C device!\n");
		exit(1);
		return -1;
	}
	return dev_h;
}

unsigned long check_ioctl(int device){
	unsigned long funcs;
	if(ioctl(device, I2C_FUNCS, &funcs) < 0){
		perror("Error: No I2C functions found!\n");
		exit(1);
		return 0;
	}
	if(funcs & I2C_FUNC_I2C){
		dbg(lvl_debug,"I2C_FUNC_I2C found.\n");
	}
	if(funcs & I2C_FUNC_SMBUS_BYTE){
		dbg(lvl_debug,"I2C_FUNC_SMBUS_BYTE found.\n");
	}	
	return funcs;
}

void scan_i2c_bus(int device){
	int port, res;
	for(port = 0; port < 127; port++){
		if(ioctl(device, I2C_SLAVE, port) < 0){
			dbg(lvl_debug,"Error: No I2C_SLAVE found!\n");
		}else{
			res = i2c_smbus_read_byte(device);
			if (res >= 0){
				dbg(lvl_debug,"I2C device found at 0x%02x, val = 0x%02x\n",port, res);
			}
		}
	}
}

int select_slave(int device, uint8_t addr){
	int res;
	dbg(lvl_debug,"Probe Address 0x%02X: %i\n", addr, ioctl(device, I2C_SLAVE, addr));
	res = i2c_smbus_read_byte_data(device, 0);
	if (res >= 0){
		dbg(lvl_debug,"I2C device found at 0x%02x, val = 0x%02x\n",addr, res);
		return 0;
	}
	return 1;
}


// Navigation_next_turn

/*
struct nav_next_turn {
	char *test_text;
	char *icon_src;
	int icon_h, icon_w, active;
	char *last_name;
	int level;
};

static void
osd_nav_next_turn_draw(struct osd_priv_common *opc, struct navit *navit,
		       struct vehicle *v)
{
	struct nav_next_turn *this = (struct nav_next_turn *)opc->data;

	struct point p;
	int do_draw = opc->osd_item.do_draw;
	struct navigation *nav = NULL;
	struct map *map = NULL;
	struct map_rect *mr = NULL;
	struct item *item = NULL;
	struct graphics_image *gr_image;
	char *image;
	char *name = "unknown";
	int level = this->level;

	if (navit)
		nav = navit_get_navigation(navit);
	if (nav)
		map = navigation_get_map(nav);
	if (map)
		mr = map_rect_new(map, NULL);
	if (mr)
		while ((item = map_rect_get_item(mr))
		       && (item->type == type_nav_position || item->type == type_nav_none || level-- > 0));
	if (item) {
		name = item_to_name(item->type);
		dbg(lvl_debug, "name=%s\n", name);
		if (this->active != 1 || this->last_name != name) {
			this->active = 1;
			this->last_name = name;
			do_draw = 1;
		}
	} else {
		if (this->active != 0) {
			this->active = 0;
			do_draw = 1;
		}
	}
	if (mr)
		map_rect_destroy(mr);

	if (do_draw) {
		osd_fill_with_bgcolor(&opc->osd_item);
		if (this->active) {
			image = g_strdup_dbg(lvl_debug,this->icon_src, name);
			dbg(lvl_debug, "image=%s\n", image);
			gr_image =
			    graphics_image_new_scaled(opc->osd_item.gr,
						      image, this->icon_w,
						      this->icon_h);
			if (!gr_image) {
				dbg(lvl_error,"failed to load %s in %dx%d\n",image,this->icon_w,this->icon_h);
				g_free(image);
				image = graphics_icon_path("unknown.png");
				gr_image =
				    graphics_image_new_scaled(opc->
							      osd_item.gr,
							      image,
							      this->icon_w,
							      this->
							      icon_h);
			}
			dbg(lvl_debug, "gr_image=%p\n", gr_image);
			if (gr_image) {
				p.x =
				    (opc->osd_item.w -
				     gr_image->width) / 2;
				p.y =
				    (opc->osd_item.h -
				     gr_image->height) / 2;
				graphics_draw_image(opc->osd_item.gr,
						    opc->osd_item.
						    graphic_fg, &p,
						    gr_image);
				graphics_image_free(opc->osd_item.gr,
						    gr_image);
			}
			g_free(image);
		}
		graphics_draw_mode(opc->osd_item.gr, draw_mode_end);
	}
}

static void
osd_nav_next_turn_init(struct osd_priv_common *opc, struct navit *nav)
{
	osd_set_std_graphic(nav, &opc->osd_item, (struct osd_priv *)opc);
	navit_add_callback(nav, callback_new_attr_1(callback_cast(osd_nav_next_turn_draw), attr_position_coord_geo, opc));
	navit_add_callback(nav, callback_new_attr_1(callback_cast(osd_std_click), attr_button, &opc->osd_item));
	osd_nav_next_turn_draw(opc, nav, NULL);
}

static struct osd_priv *
osd_nav_next_turn_new(struct navit *nav, struct osd_methods *meth,
		      struct attr **attrs)
{
	struct nav_next_turn *this = g_new0(struct nav_next_turn, 1);
	struct osd_priv_common *opc = g_new0(struct osd_priv_common,1);
	struct attr *attr;

	opc->data = (void*)this;
	opc->osd_item.rel_x = 20;
	opc->osd_item.rel_y = -80;
	opc->osd_item.rel_w = 70;
	opc->osd_item.navit = nav;
	opc->osd_item.rel_h = 70;
	opc->osd_item.font_size = 200;
	opc->osd_item.meth.draw = osd_draw_cast(osd_nav_next_turn_draw);
	meth->set_attr = set_std_osd_attr;
	osd_set_std_attr(attrs, &opc->osd_item, 0);

	this->icon_w = -1;
	this->icon_h = -1;
	this->active = -1;
	this->level  = 0;

	attr = attr_search(attrs, NULL, attr_icon_w);
	if (attr)
		this->icon_w = attr->u.num;

	attr = attr_search(attrs, NULL, attr_icon_h);
	if (attr)
		this->icon_h = attr->u.num;

	attr = attr_search(attrs, NULL, attr_icon_src);
	if (attr) {
		struct file_wordexp *we;
		char **array;
		we = file_wordexp_new(attr->u.str);
		array = file_wordexp_get_array(we);
		this->icon_src = graphics_icon_path(array[0]);
		file_wordexp_destroy(we);
	} else {
		this->icon_src = graphics_icon_path("%s_wh.svg");
	}
	
	attr = attr_search(attrs, NULL, attr_level);
	if (attr)
		this->level=attr->u.num;

	navit_add_callback(nav, callback_new_attr_1(callback_cast(osd_nav_next_turn_init), attr_graphics_ready, opc));
	return (struct osd_priv *) opc;
}
//*/


///////////////////////////////////////////////////////////////////////////
// PWM 
///////////////////////////////////////////////////////////////////////////
//*
uint8_t serialize_pwm_txdata(tx_pwm_t *tx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(tx_pwm_t)){
		dbg(lvl_debug,"size: %i, struct: %i\n",size,sizeof(tx_pwm_t));
		return 0;
	}
	dbg(lvl_debug,"\nserialize_pwm_txdata\n");
	dbg(lvl_debug,"PWM:\nfreq: %i\ntemp: %i\nvtg: %i\nwatertemp_sh: %i\ntime_sh: %i\n",tx->pwm_freq, tx->cal_temperature, tx->cal_voltage, tx->water_value, tx->time_value);
		buffer[0] = (uint8_t) ((tx->pwm_freq & 0xFF00) >> 8);
	buffer[1] = (uint8_t) (tx->pwm_freq & 0x00FF);
	buffer[2] = tx->cal_temperature;
	buffer[3] = tx->cal_voltage;
	buffer[4] = tx->water_value;
	buffer[5] = tx->time_value;
	dbg(lvl_debug,"PWM: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
	return 1;
}

uint8_t serialize_pwm_rxdata(rx_pwm_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_pwm_t)){
		return 0;
	}
	dbg(lvl_debug,"\nserialize_pwm_rxdata\n");
	dbg(lvl_debug,"PWM:\nfreq: %i\ntemp: %i\nvtg: %i\nwatertemp_sh: %i\ntime_sh: %i\nvbat: %i\nwatertemp: %i\nfettemp:%i\n",rx->pwm_freq, rx->cal_temperature, rx->cal_voltage, rx->water_value, rx->time_value, rx->vbat, rx->water_temp, rx->fet_temp);
	buffer[0] = (uint8_t) ((rx->pwm_freq & 0xFF00) >> 8);
	buffer[1] = (uint8_t) (rx->pwm_freq & 0x00FF);
	buffer[2] = rx->cal_temperature;
	buffer[3] = rx->cal_voltage;
	buffer[4] = rx->water_value;
	buffer[5] = rx->time_value;
	buffer[6] = (uint8_t) ((rx->vbat & 0xFF00) >> 8);
	buffer[7] = (uint8_t) (rx->vbat & 0x00FF);
	buffer[8] = rx->water_temp;
	buffer[9] = rx->fet_temp;
	dbg(lvl_debug,"PWM: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	
	return 1;
}

uint8_t deserialize_pwm_rxdata(rx_pwm_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_pwm_t)){
		return 0;
	}
	dbg(lvl_debug,"\ndeserialize_pwm_rxdata\n");
	dbg(lvl_debug,"PWM: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	rx->pwm_freq = ((uint16_t) (buffer[0]) << 8) + buffer[1];
	rx->cal_temperature = buffer[2];
	rx->cal_voltage = buffer[3];
	rx->water_value = buffer[4];
	rx->time_value = buffer[5];
	rx->vbat = ((uint16_t) (buffer[6]) << 8) + buffer[7];
	rx->water_temp = buffer[8];
	rx->fet_temp = buffer[9];
	dbg(lvl_debug,"PWM:\nfreq: %i\ntemp: %i\nvtg: %i\nwatertemp_sh: %i\ntime_sh: %i\nvbat: %i\nwatertemp: %i\nfettemp:%i\n",rx->pwm_freq, rx->cal_temperature, rx->cal_voltage, rx->water_value, rx->time_value, rx->vbat, rx->water_temp, rx->fet_temp);
	return 1;
}

uint8_t pwm_rx_task(int device){
	uint8_t i;
	uint8_t rx_size = sizeof(rx_pwm_t);
	uint8_t i2crxdata[rx_size+1];
	dbg(lvl_debug,"Read i2c\n");
	read_i2c_frame(device, i2crxdata, rx_size+1);
	
	uint8_t rx_crc = calculateCRC8(CRC_POLYNOME, i2crxdata, rx_size);
	dbg(lvl_debug,"// check rx crc: 0x%02X 0x%02X\n",rx_crc,i2crxdata[rx_size]);
	if(rx_crc == i2crxdata[rx_size]){
		dbg(lvl_debug,"//crc is correct\n");
		uint8_t ser_rx[rx_size];
		
		if(serialize_pwm_rxdata(rx_pwm, rx_size, ser_rx)){
			dbg(lvl_debug,"// check if new data differs from current data object\n");
			uint8_t ok = 0;
			for(i=0; i<rx_size; i++){
				dbg(lvl_debug,"check i2crxdata[%i] 0x%02X != ser_rx[%i] 0x%02X\n",i,i2crxdata[i],i,ser_rx[i]);
				if(i2crxdata[i] != ser_rx[i]) ok = 1;
			}
			if(ok){
				dbg(lvl_debug,"//we got new data -> replace the old object \n");
				if(!deserialize_pwm_rxdata(rx_pwm, rx_size, i2crxdata)){
					dbg(lvl_debug,"failed to replace\n");
					return 1;
				}else{
					dbg(lvl_debug,"// everything went fine -> clean the buffer\n");
					for(i=0; i <= rx_size; i++){
						i2crxdata[i] = 0;
					}
				}
			}else return 1;
		}else return 1;
	}else return 1;
	dbg(lvl_debug,"// end of rx data procession\n");
	return 0;
	
}	
uint8_t pwm_tx_task(int device){
	uint8_t i;
	uint8_t tx_size = sizeof(tx_pwm_t);	
	uint8_t i2ctxdata[tx_size+1];	
	dbg(lvl_debug,"// serialize tx object %i\n", tx_size);
	if(serialize_pwm_txdata(tx_pwm, tx_size, i2ctxdata)){
		dbg(lvl_debug,"//calculate CRC and append to i2ctxdata\n");
		i2ctxdata[tx_size] = calculateCRC8(CRC_POLYNOME, i2ctxdata, tx_size);
		for(i=0;i<tx_size;i++){
			i2c_smbus_write_byte_data(device, i, i2ctxdata[i]);
			dbg(lvl_debug,"Write %i: 0x%02X\n", i, i2ctxdata[i]);
		}
		i2c_smbus_write_byte_data(device, i,i2ctxdata[tx_size]); 
		dbg(lvl_debug,"Write CRC: 0x%02X\n", i2ctxdata[tx_size]);
	}else return 1;
	return 0;
}


//*////////////////////////////////////////////////////////////////////////
// MFA 
///////////////////////////////////////////////////////////////////////////
/*
uint8_t serialize_mfa_txdata(tx_mfa_t *tx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(tx_mfa_t)){
		dbg(lvl_debug,"size: %i, struct: %i\n",size,sizeof(tx_mfa_t));
		return 0;
	}
	dbg(lvl_debug,"\nserialize_mfa_txdata\n");
	uint8_t i;
	for(i=0;i<32;i++){
		buffer[i] = rx->radio_text[i];
	}
	buffer[32] = rx->navigation_next_turn;//or status
	buffer[33] = (uint8_t) ((rx->distance_to_next_turn & 0xFF000000) >> 24);
	buffer[34] = (uint8_t) ((rx->distance_to_next_turn & 0x00FF0000) >> 16);
	buffer[35] = (uint8_t) ((rx->distance_to_next_turn & 0x0000FF00) >> 8);
	buffer[36] = (uint8_t) ((rx->distance_to_next_turn & 0x000000FF));
	//navigation active?
	buffer[37] = rx->cal_water_temperature;
	buffer[38] = rx->cal_voltage;
	buffer[39] = rx->cal_oil_temperature;
	buffer[40] = rx->cal_consumption;	
	dbg(lvl_debug,"mfa: ");
	for(i=0;i<size; i++){
		dbg(lvl_debug,"0x%02X ",buffer[i]);
	}
	dbg(lvl_debug,"\n");
	return 1;
}

uint8_t serialize_mfa_rxdata(rx_mfa_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_mfa_t)){
		return 0;
	}
	dbg(lvl_debug,"\nserialize_mfa_rxdata\n");
	uint8_t i;
	for(i=0;i<32;i++){
		buffer[i] = rx->radio_text[i];
	}
	buffer[32] = rx->navigation_next_turn;//or status
	buffer[33] = (uint8_t) ((rx->distance_to_next_turn & 0xFF000000) >> 24);
	buffer[34] = (uint8_t) ((rx->distance_to_next_turn & 0x00FF0000) >> 16);
	buffer[35] = (uint8_t) ((rx->distance_to_next_turn & 0x0000FF00) >> 8);
	buffer[36] = (uint8_t) ((rx->distance_to_next_turn & 0x000000FF));
	//navigation active?
	buffer[37] = rx->cal_water_temperature;
	buffer[38] = rx->cal_voltage;
	buffer[39] = rx->cal_oil_temperature;
	buffer[40] = rx->cal_consumption;
	// read only
	buffer[41] = (uint8_t) ((rx->voltage & 0xFF00) >> 8);
	buffer[42] = (uint8_t) ((rx->voltage & 0x00FF));
	buffer[43] = rx->water_temperature;
	buffer[44] = rx->ambient_temperature;
	buffer[45] = rx->oil_temperature;
	buffer[46] = ((uint8_t) ((rx->consumption & 0xFF00) >> 8);
	buffer[47] = ((uint8_t) ((rx->consumption & 0x00FF));
	buffer[48] = ((uint8_t) ((rx->average_consumption & 0xFF00) >> 8);
	buffer[49] = ((uint8_t) ((rx->average_consumption & 0x00FF));
	buffer[50] = ((uint8_t) ((rx->range & 0xFF00) >> 8);
	buffer[51] = ((uint8_t) ((rx->range & 0x00FF));
	buffer[52] = ((uint8_t) ((rx->speed & 0xFF00) >> 8);
	buffer[53] = ((uint8_t) ((rx->speed & 0x00FF));
	buffer[54] = ((uint8_t) ((rx->average_speed & 0xFF00) >> 8);
	buffer[55] = ((uint8_t) ((rx->average_speed & 0x00FF));
	buffer[56] = ((uint8_t) ((rx->rpm & 0xFF00) >> 8);
	buffer[57] = ((uint8_t) ((rx->rpm & 0x00FF));
	
	dbg(lvl_debug,"mfa: ");
	for(i=0;i<size; i++){
		dbg(lvl_debug,"0x%02X ",buffer[i]);
	}
	dbg(lvl_debug,"\n");
	return 1;
}

uint8_t deserialize_mfa_rxdata(rx_mfa_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_mfa_t)){
		return 0;
	}
	dbg(lvl_debug,"\ndeserialize_mfa_rxdata\n");
	dbg(lvl_debug,"mfa: ");
	for(i=0;i<size; i++){
		dbg(lvl_debug,"0x%02X ",buffer[i]);
	}
	dbg(lvl_debug,"\n");
	
	for(i=0;i<32;i++){
		rx->radio_text[i] = buffer[i];
	}
	
	rx->navigation_next_turn = buffer[32];
	rx->distance_to_next_turn = (long) buffer[33] << 24 + (long) buffer[34] << 16 + (long) buffer[35] << 8 + buffer[36];
	//navigation active?
	rx->cal_water_temperature = buffer[37];
	rx->cal_voltage = buffer[38];
	rx->cal_oil_temperature = buffer[39];
	rx->cal_consumption = buffer[40];
	// read only
	rx->voltage = (uint16_t) buffer[41] << 8 + buffer[42];
	rx->water_temperature = buffer[43];
	rx->ambient_temperature = buffer[44];
	rx->oil_temperature = buffer[45];
	rx->consumption = (uint16_t) buffer[46] << 8 + buffer[47];
	rx->average_consumption = (uint16_t) buffer[48] << 8 + buffer[49];
	rx->range = (uint16_t) buffer[50] << 8 + buffer[51];
	rx->speed = (uint16_t) buffer[52] << 8 + buffer[53];
	rx->average_speed = (uint16_t) buffer[54] << 8 + buffer[55];
	rx->rpm = (uint16_t) buffer[56] << 8 + buffer[57];
	return 1;
}

uint8_t mfa_rx_task(int device){
	uint8_t i;
	uint8_t rx_size = sizeof(rx_mfa_t);
	uint8_t i2crxdata[rx_size+1];
	dbg(lvl_debug,"Read i2c\n");
	read_i2c_frame(device, i2crxdata, rx_size+1);
	
	uint8_t rx_crc = calculateCRC8(CRC_POLYNOME, i2crxdata, rx_size);
	dbg(lvl_debug,"// check rx crc: 0x%02X 0x%02X\n",rx_crc,i2crxdata[rx_size]);
	if(rx_crc == i2crxdata[rx_size]){
		dbg(lvl_debug,"//crc is correct\n");
		uint8_t ser_rx[rx_size];
		
		if(serialize_mfa_rxdata(rx_mfa, rx_size, ser_rx)){
			dbg(lvl_debug,"// check if new data differs from current data object\n");
			uint8_t ok = 0;
			for(i=0; i<rx_size; i++){
				dbg(lvl_debug,"check i2crxdata[%i] 0x%02X != ser_rx[%i] 0x%02X\n",i,i2crxdata[i],i,ser_rx[i]);
				if(i2crxdata[i] != ser_rx[i]) ok = 1;
			}
			if(ok){
				dbg(lvl_debug,"//we got new data -> replace the old object \n");
				if(!deserialize_mfa_rxdata(rx_mfa, rx_size, i2crxdata)){
					dbg(lvl_debug,"failed to replace\n");
					return 1;
				}else{
					dbg(lvl_debug,"// everything went fine -> clean the buffer\n");
					for(i=0; i <= rx_size; i++){
						i2crxdata[i] = 0;
					}
				}
			}else return 1;
		}else return 1;
	}else return 1;
	dbg(lvl_debug,"// end of rx data procession\n");
	return 0;
	
}	
uint8_t mfa_tx_task(int device){
	uint8_t i;
	uint8_t tx_size = sizeof(tx_mfa_t);	
	uint8_t i2ctxdata[tx_size+1];	
	dbg(lvl_debug,"// serialize tx object %i\n", tx_size);
	if(serialize_mfa_txdata(tx_mfa, tx_size, i2ctxdata)){
		dbg(lvl_debug,"//calculate CRC and append to i2ctxdata\n");
		i2ctxdata[tx_size] = calculateCRC8(CRC_POLYNOME, i2ctxdata, tx_size);
		for(i=0;i<tx_size;i++){
			i2c_smbus_write_byte_data(device, i, i2ctxdata[i]);
			dbg(lvl_debug,"Write %i: 0x%02X\n", i, i2ctxdata[i]);
		}
		i2c_smbus_write_byte_data(device, i,i2ctxdata[tx_size]); 
		dbg(lvl_debug,"Write CRC: 0x%02X\n", i2ctxdata[tx_size]);
	}else return 1;
	return 0;
}

//*////////////////////////////////////////////////////////////////////////
// LSG 
///////////////////////////////////////////////////////////////////////////
/*
uint8_t serialize_lsg_txdata(tx_lsg_t *tx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(tx_lsg_t)){
		dbg(lvl_debug,"size: %i, struct: %i\n",size,sizeof(tx_lsg_t));
		return 0;
	}
	dbg(lvl_debug,"\nserialize_lsg_txdata\n");
	dbg(lvl_debug,"lsg: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
	return 1;
}

uint8_t serialize_lsg_rxdata(rx_lsg_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_lsg_t)){
		return 0;
	}
	dbg(lvl_debug,"\nserialize_lsg_rxdata\n");
	dbg(lvl_debug,"lsg: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	
	return 1;
}

uint8_t deserialize_lsg_rxdata(rx_lsg_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_lsg_t)){
		return 0;
	}
	dbg(lvl_debug,"\ndeserialize_lsg_rxdata\n");
	dbg(lvl_debug,"lsg: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	return 1;
}

uint8_t lsg_rx_task(int device){
	uint8_t i;
	uint8_t rx_size = sizeof(rx_lsg_t);
	uint8_t i2crxdata[rx_size+1];
	dbg(lvl_debug,"Read i2c\n");
	read_i2c_frame(device, i2crxdata, rx_size+1);
	
	uint8_t rx_crc = calculateCRC8(CRC_POLYNOME, i2crxdata, rx_size);
	dbg(lvl_debug,"// check rx crc: 0x%02X 0x%02X\n",rx_crc,i2crxdata[rx_size]);
	if(rx_crc == i2crxdata[rx_size]){
		dbg(lvl_debug,"//crc is correct\n");
		uint8_t ser_rx[rx_size];
		
		if(serialize_lsg_rxdata(rx_lsg, rx_size, ser_rx)){
			dbg(lvl_debug,"// check if new data differs from current data object\n");
			uint8_t ok = 0;
			for(i=0; i<rx_size; i++){
				dbg(lvl_debug,"check i2crxdata[%i] 0x%02X != ser_rx[%i] 0x%02X\n",i,i2crxdata[i],i,ser_rx[i]);
				if(i2crxdata[i] != ser_rx[i]) ok = 1;
			}
			if(ok){
				dbg(lvl_debug,"//we got new data -> replace the old object \n");
				if(!deserialize_lsg_rxdata(rx_lsg, rx_size, i2crxdata)){
					dbg(lvl_debug,"failed to replace\n");
					return 1;
				}else{
					dbg(lvl_debug,"// everything went fine -> clean the buffer\n");
					for(i=0; i <= rx_size; i++){
						i2crxdata[i] = 0;
					}
				}
			}else return 1;
		}else return 1;
	}else return 1;
	dbg(lvl_debug,"// end of rx data procession\n");
	return 0;
	
}	
uint8_t lsg_tx_task(int device){
	uint8_t i;
	uint8_t tx_size = sizeof(tx_lsg_t);	
	uint8_t i2ctxdata[tx_size+1];	
	dbg(lvl_debug,"// serialize tx object %i\n", tx_size);
	if(serialize_lsg_txdata(tx_lsg, tx_size, i2ctxdata)){
		dbg(lvl_debug,"//calculate CRC and append to i2ctxdata\n");
		i2ctxdata[tx_size] = calculateCRC8(CRC_POLYNOME, i2ctxdata, tx_size);
		for(i=0;i<tx_size;i++){
			i2c_smbus_write_byte_data(device, i, i2ctxdata[i]);
			dbg(lvl_debug,"Write %i: 0x%02X\n", i, i2ctxdata[i]);
		}
		i2c_smbus_write_byte_data(device, i,i2ctxdata[tx_size]); 
		dbg(lvl_debug,"Write CRC: 0x%02X\n", i2ctxdata[tx_size]);
	}else return 1;
	return 0;
}

//*////////////////////////////////////////////////////////////////////////
// WFS
///////////////////////////////////////////////////////////////////////////
/*
uint8_t serialize_wfs_txdata(tx_wfs_t *tx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(tx_wfs_t)){
		dbg(lvl_debug,"size: %i, struct: %i\n",size,sizeof(tx_wfs_t));
		return 0;
	}
	dbg(lvl_debug,"\nserialize_wfs_txdata\n");
	dbg(lvl_debug,"wfs: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
	return 1;
}

uint8_t serialize_wfs_rxdata(rx_wfs_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_wfs_t)){
		return 0;
	}
	dbg(lvl_debug,"\nserialize_wfs_rxdata\n");
	dbg(lvl_debug,"wfs: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	
	return 1;
}

uint8_t deserialize_wfs_rxdata(rx_wfs_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_wfs_t)){
		return 0;
	}
	dbg(lvl_debug,"\ndeserialize_wfs_rxdata\n");
	dbg(lvl_debug,"wfs: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	return 1;
}

uint8_t wfs_rx_task(int device){
	uint8_t i;
	uint8_t rx_size = sizeof(rx_wfs_t);
	uint8_t i2crxdata[rx_size+1];
	dbg(lvl_debug,"Read i2c\n");
	read_i2c_frame(device, i2crxdata, rx_size+1);
	
	uint8_t rx_crc = calculateCRC8(CRC_POLYNOME, i2crxdata, rx_size);
	dbg(lvl_debug,"// check rx crc: 0x%02X 0x%02X\n",rx_crc,i2crxdata[rx_size]);
	if(rx_crc == i2crxdata[rx_size]){
		dbg(lvl_debug,"//crc is correct\n");
		uint8_t ser_rx[rx_size];
		
		if(serialize_wfs_rxdata(rx_wfs, rx_size, ser_rx)){
			dbg(lvl_debug,"// check if new data differs from current data object\n");
			uint8_t ok = 0;
			for(i=0; i<rx_size; i++){
				dbg(lvl_debug,"check i2crxdata[%i] 0x%02X != ser_rx[%i] 0x%02X\n",i,i2crxdata[i],i,ser_rx[i]);
				if(i2crxdata[i] != ser_rx[i]) ok = 1;
			}
			if(ok){
				dbg(lvl_debug,"//we got new data -> replace the old object \n");
				if(!deserialize_wfs_rxdata(rx_wfs, rx_size, i2crxdata)){
					dbg(lvl_debug,"failed to replace\n");
					return 1;
				}else{
					dbg(lvl_debug,"// everything went fine -> clean the buffer\n");
					for(i=0; i <= rx_size; i++){
						i2crxdata[i] = 0;
					}
				}
			}else return 1;
		}else return 1;
	}else return 1;
	dbg(lvl_debug,"// end of rx data procession\n");
	return 0;
	
}	
uint8_t wfs_tx_task(int device){
	uint8_t i;
	uint8_t tx_size = sizeof(tx_wfs_t);	
	uint8_t i2ctxdata[tx_size+1];	
	dbg(lvl_debug,"// serialize tx object %i\n", tx_size);
	if(serialize_wfs_txdata(tx_wfs, tx_size, i2ctxdata)){
		dbg(lvl_debug,"//calculate CRC and append to i2ctxdata\n");
		i2ctxdata[tx_size] = calculateCRC8(CRC_POLYNOME, i2ctxdata, tx_size);
		for(i=0;i<tx_size;i++){
			i2c_smbus_write_byte_data(device, i, i2ctxdata[i]);
			dbg(lvl_debug,"Write %i: 0x%02X\n", i, i2ctxdata[i]);
		}
		i2c_smbus_write_byte_data(device, i,i2ctxdata[tx_size]); 
		dbg(lvl_debug,"Write CRC: 0x%02X\n", i2ctxdata[tx_size]);
	}else return 1;
	return 0;
}

//*////////////////////////////////////////////////////////////////////////
// V2V 
///////////////////////////////////////////////////////////////////////////
/*
uint8_t serialize_v2v_txdata(tx_v2v_t *tx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(tx_v2v_t)){
		dbg(lvl_debug,"size: %i, struct: %i\n",size,sizeof(tx_v2v_t));
		return 0;
	}
	dbg(lvl_debug,"\nserialize_v2v_txdata\n");
	dbg(lvl_debug,"v2v: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
	return 1;
}

uint8_t serialize_v2v_rxdata(rx_v2v_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_v2v_t)){
		return 0;
	}
	dbg(lvl_debug,"\nserialize_v2v_rxdata\n");
	dbg(lvl_debug,"v2v: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	
	return 1;
}

uint8_t deserialize_v2v_rxdata(rx_v2v_t *rx, uint8_t size, volatile uint8_t buffer[size]){
	if(size != sizeof(rx_v2v_t)){
		return 0;
	}
	dbg(lvl_debug,"\ndeserialize_v2v_rxdata\n");
	dbg(lvl_debug,"v2v: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
	return 1;
}

uint8_t v2v_rx_task(int device){
	uint8_t i;
	uint8_t rx_size = sizeof(rx_v2v_t);
	uint8_t i2crxdata[rx_size+1];
	dbg(lvl_debug,"Read i2c\n");
	read_i2c_frame(device, i2crxdata, rx_size+1);
	
	uint8_t rx_crc = calculateCRC8(CRC_POLYNOME, i2crxdata, rx_size);
	dbg(lvl_debug,"// check rx crc: 0x%02X 0x%02X\n",rx_crc,i2crxdata[rx_size]);
	if(rx_crc == i2crxdata[rx_size]){
		dbg(lvl_debug,"//crc is correct\n");
		uint8_t ser_rx[rx_size];
		
		if(serialize_v2v_rxdata(rx_v2v, rx_size, ser_rx)){
			dbg(lvl_debug,"// check if new data differs from current data object\n");
			uint8_t ok = 0;
			for(i=0; i<rx_size; i++){
				dbg(lvl_debug,"check i2crxdata[%i] 0x%02X != ser_rx[%i] 0x%02X\n",i,i2crxdata[i],i,ser_rx[i]);
				if(i2crxdata[i] != ser_rx[i]) ok = 1;
			}
			if(ok){
				dbg(lvl_debug,"//we got new data -> replace the old object \n");
				if(!deserialize_v2v_rxdata(rx_v2v, rx_size, i2crxdata)){
					dbg(lvl_debug,"failed to replace\n");
					return 1;
				}else{
					dbg(lvl_debug,"// everything went fine -> clean the buffer\n");
					for(i=0; i <= rx_size; i++){
						i2crxdata[i] = 0;
					}
				}
			}else return 1;
		}else return 1;
	}else return 1;
	dbg(lvl_debug,"// end of rx data procession\n");
	return 0;
	
}	
uint8_t v2v_tx_task(int device){
	uint8_t i;
	uint8_t tx_size = sizeof(tx_v2v_t);	
	uint8_t i2ctxdata[tx_size+1];	
	dbg(lvl_debug,"// serialize tx object %i\n", tx_size);
	if(serialize_v2v_txdata(tx_v2v, tx_size, i2ctxdata)){
		dbg(lvl_debug,"//calculate CRC and append to i2ctxdata\n");
		i2ctxdata[tx_size] = calculateCRC8(CRC_POLYNOME, i2ctxdata, tx_size);
		for(i=0;i<tx_size;i++){
			i2c_smbus_write_byte_data(device, i, i2ctxdata[i]);
			dbg(lvl_debug,"Write %i: 0x%02X\n", i, i2ctxdata[i]);
		}
		i2c_smbus_write_byte_data(device, i,i2ctxdata[tx_size]); 
		dbg(lvl_debug,"Write CRC: 0x%02X\n", i2ctxdata[tx_size]);
	}else return 1;
	return 0;
}



//*////////////////////////////////////////////////////////////////////////
// TEST 
///////////////////////////////////////////////////////////////////////////


void read_i2c_frame(int device, uint8_t* data, uint8_t size){
	uint8_t i;
	dbg(lvl_debug,"Read %i bytes\n", size);
	for(i=0;i<size;i++){	
		if(i==0){
			data[i] = i2c_smbus_read_byte_data(device, i);
		}else{
			data[i] = i2c_smbus_read_byte(device);
		}
		dbg(lvl_debug,"Read %i: 0x%02X 0x%02X\n", i, (uint8_t) data[i], device);
	}
}

void test_i2c(int device, uint8_t* addr, uint8_t addr_size){
	uint8_t j,i;
	for(j=0; j<addr_size; j++){
		if(select_slave(device, addr[j])){
			dbg(lvl_error,"Can't open port: 0x%02X\n",addr[j]);

		}else{
			for(i=0; i<6; i++){
				if(i==0){
					result[i]=i2c_smbus_read_byte_data(device, i);
				}else{
					result[i] = i2c_smbus_read_byte(device);
				}
				dbg(lvl_debug,"Read %i: 0x%02X 0x%02X\n", i, (uint8_t) result[i], device);
			}
		
			for(i=0; i<6; i++){
					i2c_smbus_write_byte_data(device, i, x[i]);
					dbg(lvl_debug,"Write %i: 0x%02X 0x%02X\n", i, x[i], device);
			}
			for(i=0; i<6; i++){
				x[i]=result[i];
			}
		}
	}
}
/*
void pwm_tx_task(int device){
	dbg(lvl_debug, "pwm_tx_task\n");
}

void pwm_rx_task(int device){
	dbg(lvl_debug, "pwm_rx_task\n");
}
*/
///////////////////////////////////////////////////////////////////////////
// MAIN
///////////////////////////////////////////////////////////////////////////

static void 
i2c_task(struct i2c *this){
	select_slave(this->device, this->addr[2]);
	uint8_t size = sizeof(this->addr); 
	uint8_t i;
	//for(i=0; i<size;i++){
		dbg(lvl_error, "%i\n\n\n ", i);
		pwm_rx_task(this->device);
		//getchar();
		
		tx_pwm->pwm_freq += 500;
		if(tx_pwm->pwm_freq > 65000) tx_pwm->pwm_freq = 1000;
		
		pwm_tx_task(this->device);
		//getchar();
	//}
}

static void 
i2c_main(struct i2c *this, struct navit *nav){
	
	this->callback = callback_new_1(callback_cast(i2c_task), this);
	this->timeout = event_add_timeout(5000, 1, this->callback);
	
	//pTODO: init i2c data types
	tx_pwm->pwm_freq = 10000;
	tx_pwm->cal_temperature = 5;
	tx_pwm->cal_voltage = 5;
	tx_pwm->time_value = 10;
	tx_pwm->water_value = 35;
	
	return;
}

static void 
i2c_init(struct i2c *this, struct navit *nav)
{
	dbg(lvl_error, "i2c_init\n");
	this->nav=nav;

	init_i2c_data();
	dbg(lvl_debug,"I2C Test\n");
	this->device = open_i2c("/dev/i2c-1");
	check_ioctl(this->device);
	scan_i2c_bus(this->device);

	uint8_t addr[5];
	dbg(lvl_debug,"MFA: 0x%02x\n", calculateID("MFA"));
	this->addr[0] = calculateID("MFA");
	dbg(lvl_debug,"PWM: 0x%02x\n", calculateID("PWM"));
	this->addr[1] = calculateID("PWM");
	dbg(lvl_debug,"ABC: 0x%02x\n", calculateID("ABC"));
	this->addr[2] = calculateID("ABC");
	dbg(lvl_debug,"WFS: 0x%02x\n", calculateID("WFS"));
	this->addr[3] = calculateID("WFS");
	dbg(lvl_debug,"LSG: 0x%02x\n", calculateID("LSG"));
	this->addr[4] = calculateID("LSG");
	navit_add_callback(nav,callback_new_attr_1(callback_cast(i2c_main),attr_graphics_ready, this));
}



void
plugin_init(void)
{
	struct attr callback; 
	struct i2c_plugin *this=g_new0(struct i2c, 1);
	callback.type=attr_callback;
	callback.u.callback=callback_new_attr_1(callback_cast(i2c_init),attr_navit,this);
	config_add_attr(config, &callback);
	dbg(lvl_debug,"hello i2c\n\n");
}

