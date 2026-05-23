// SPDX-License-Identifier: GPL-3.0-or-later
#include "vu_meter.h"
#include "hdsp_device.h"

#include <FL/fl_draw.H>
#include <math.h>
#include <algorithm>

VuMeter::VuMeter(int x, int y, int w, int h, const char *label)
    : Fl_Widget(x, y, w, h, label),
      level_db_(kFloor),
      peak_hold_db_(kFloor),
      peak_hold_ttl_(0)
{
}

void VuMeter::set_db(float db) {
	if (db < kFloor) db = kFloor;
	if (db > 0.0f)   db = 0.0f;
	level_db_ = db;

	if (db >= peak_hold_db_) {
		peak_hold_db_ = db;
		peak_hold_ttl_ = kHoldFrames;
	}
	damage(FL_DAMAGE_ALL);
}

void VuMeter::set_peak_raw(uint32_t peak) {
	set_db(HdspDevice::peak_to_db(peak));
}

void VuMeter::tick() {
	if (peak_hold_ttl_ > 0) {
		--peak_hold_ttl_;
	} else if (peak_hold_db_ > kFloor) {
		peak_hold_db_ -= 1.5f; /* decay 1.5 dB per frame */
		if (peak_hold_db_ < kFloor)
			peak_hold_db_ = kFloor;
		damage(FL_DAMAGE_ALL);
	}
}

float VuMeter::db_to_frac(float db) const {
	/* Linear mapping: kFloor → 0.0, 0 dBFS → 1.0 */
	return (db - kFloor) / (-kFloor);
}

void VuMeter::draw() {
	const int bx = x(), by = y(), bw = w(), bh = h();

	/* Background */
	fl_color(FL_BLACK);
	fl_rectf(bx, by, bw, bh);

	if (bh < 4 || bw < 2)
		return;

	/* Reserve label area at bottom */
	const int lh = label() ? 12 : 0;
	const int meter_h = bh - lh;

	const float frac  = db_to_frac(level_db_);
	const int   fill  = (int)(frac * meter_h);

	/* Segment thresholds (fractions of full scale) */
	const float kGreenTop  = 0.67f; /* -18 dBFS */
	const float kYellowTop = 0.90f; /* -6 dBFS */

	/* Draw bar from the bottom up */
	for (int row = 0; row < fill; ++row) {
		float pos = (float)row / meter_h;
		Fl_Color col;
		if (pos < kGreenTop)
			col = fl_rgb_color(0, 210, 0);
		else if (pos < kYellowTop)
			col = fl_rgb_color(230, 200, 0);
		else
			col = fl_rgb_color(230, 30, 30);

		fl_color(col);
		int bar_y = by + meter_h - 1 - row;
		fl_line(bx + 1, bar_y, bx + bw - 2, bar_y);
	}

	/* Segment gap lines (every 3 dB) */
	fl_color(FL_BLACK);
	for (float db = -60.0f; db <= 0.0f; db += 3.0f) {
		float f = db_to_frac(db);
		int gy = by + meter_h - (int)(f * meter_h);
		if (gy >= by && gy < by + meter_h)
			fl_line(bx, gy, bx + bw - 1, gy);
	}

	/* Peak hold line */
	if (peak_hold_db_ > kFloor) {
		float pf   = db_to_frac(peak_hold_db_);
		int   py   = by + meter_h - (int)(pf * meter_h) - 1;
		fl_color(FL_WHITE);
		fl_line(bx + 1, py, bx + bw - 2, py);
	}

	/* Border */
	fl_color(fl_rgb_color(60, 60, 60));
	fl_rect(bx, by, bw, meter_h);

	/* Label */
	if (lh > 0) {
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 9);
		fl_draw(label(), bx, by + meter_h, bw, lh,
		        FL_ALIGN_CENTER | FL_ALIGN_CLIP);
	}
}
