// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Output.H>
#include <vector>
#include <memory>
#include "hdsp_device.h"
#include "vu_meter.h"
#include "matrix_widget.h"

/*
 * MixerWindow — top-level FLTK window for the bsm_hdsp tool.
 *
 * Tabs
 * ----
 *   "Matrix"  — heat-map gain matrix + selected-cell fader panel
 *   "Meters"  — VU meter strips for all inputs, playbacks, outputs
 *
 * Menu
 * ----
 *   Device > (scanned device list)
 *   File   > Load Config / Save Config / Quit
 */
class MixerWindow : public Fl_Double_Window {
public:
	MixerWindow(int w, int h, const char *title = "bsm_hdsp");
	~MixerWindow();

	/* Connect to a device path (e.g. "/dev/hdsp0"). */
	void connect(const char *path);

	/* Disconnect current device. */
	void disconnect();

private:
	/* Device */
	HdspDevice  dev_;
	bool        connected_;

	/* UI widgets (allocated per-connection in build_ui()) */
	Fl_Menu_Bar   *menu_;
	Fl_Tabs       *tabs_;

	/* Matrix tab */
	Fl_Group      *tab_matrix_;
	MatrixWidget  *matrix_;
	Fl_Slider     *cell_fader_;
	Fl_Box        *cell_label_;
	Fl_Box        *cell_db_;

	/* Meters tab */
	Fl_Group      *tab_meters_;
	Fl_Scroll     *meter_scroll_;
	std::vector<VuMeter *> vu_in_;
	std::vector<VuMeter *> vu_pb_;
	std::vector<VuMeter *> vu_out_;

	/* Status bar */
	Fl_Box        *status_bar_;

	/* Build/destroy the card-specific UI elements */
	void build_ui();
	void clear_meters();

	/* Timer callback (25 Hz meter updates) */
	static void timer_cb(void *userdata);
	void        update_meters();

	/* Menu callbacks */
	static void menu_cb(Fl_Widget *w, void *data);
	void build_device_menu();
	void load_config();
	void save_config();

	/* Cell fader callback */
	static void fader_cb(Fl_Widget *w, void *data);
	void        on_cell_selected(uint32_t addr, uint16_t gain);
	void        on_fader_moved();

	uint32_t selected_addr_;

	/* Status line */
	void set_status(const char *msg);
};
