// SPDX-License-Identifier: GPL-3.0-or-later
#include "matrix_widget.h"

#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <algorithm>
#include <cstdio>

MatrixWidget::MatrixWidget(int x, int y, int w, int h)
    : Fl_Widget(x, y, w, h),
      nch_(0), nsrc_(0),
      scroll_x_(0), scroll_y_(0),
      sel_addr_(-1)
{
}

void MatrixWidget::init(int nch, const uint16_t *matrix) {
	nch_  = nch;
	nsrc_ = nch * 2;
	scroll_x_ = scroll_y_ = 0;
	sel_addr_ = -1;

	gains_.assign(HDSP_MATRIX_MIXER_SIZE, 0);
	if (matrix)
		std::copy(matrix, matrix + HDSP_MATRIX_MIXER_SIZE, gains_.begin());

	damage(FL_DAMAGE_ALL);
}

void MatrixWidget::set_gain(uint32_t addr, uint16_t gain) {
	if (addr < HDSP_MATRIX_MIXER_SIZE)
		gains_[addr] = gain;
	damage(FL_DAMAGE_ALL);
}

uint16_t MatrixWidget::get_gain(uint32_t addr) const {
	if (addr < HDSP_MATRIX_MIXER_SIZE)
		return gains_[addr];
	return 0;
}

void MatrixWidget::set_matrix(const uint16_t *matrix) {
	if (matrix)
		std::copy(matrix, matrix + HDSP_MATRIX_MIXER_SIZE, gains_.begin());
	damage(FL_DAMAGE_ALL);
}

uint32_t MatrixWidget::addr_of(int src_row, int out_col) const {
	if (src_row < nch_)
		/* playback row */
		return HDSP_PLAYBACK_ADDR(out_col, src_row);
	else
		/* capture/input row */
		return HDSP_CAPTURE_ADDR(out_col, src_row - nch_);
}

bool MatrixWidget::cell_at_pixel(int px, int py,
    int &src_row, int &out_col) const
{
	int cx = px - content_x();
	int cy = py - content_y();
	if (cx < 0 || cy < 0)
		return false;
	out_col = cx / kCellW;
	src_row = cy / kCellH;
	return out_col < nch_ && src_row < nsrc_;
}

int MatrixWidget::max_scroll_x() const {
	int avail = w() - kLabelW;
	int total = content_w();
	return std::max(0, total - avail);
}

int MatrixWidget::max_scroll_y() const {
	int avail = h() - kHdrH;
	int total = content_h();
	return std::max(0, total - avail);
}

void MatrixWidget::scroll_by(int dx, int dy) {
	scroll_x_ = std::max(0, std::min(scroll_x_ + dx, max_scroll_x()));
	scroll_y_ = std::max(0, std::min(scroll_y_ + dy, max_scroll_y()));
	damage(FL_DAMAGE_ALL);
}

Fl_Color MatrixWidget::gain_color(uint16_t gain) const {
	if (gain == 0)
		return fl_rgb_color(20, 20, 20);

	/* Map 0..HDSP_GAIN_UNITY to brightness 30..220 */
	int bright = 30 + (int)((float)gain / HDSP_GAIN_UNITY * 190.0f);
	if (bright > 220) bright = 220;

	/* Colour: teal-ish for normal gains, slightly warmer at unity */
	if (gain >= HDSP_GAIN_UNITY - 100)
		return fl_rgb_color(bright / 2, bright, bright / 2);  /* green at unity */
	return fl_rgb_color(0, bright * 3 / 4, bright);
}

void MatrixWidget::draw_cell(int src_row, int out_col, bool selected) {
	int cx = content_x() + out_col * kCellW;
	int cy = content_y() + src_row * kCellH;

	uint32_t addr  = addr_of(src_row, out_col);
	uint16_t gain  = (addr < (uint32_t)gains_.size()) ? gains_[addr] : 0;

	Fl_Color col = gain_color(gain);
	fl_color(col);
	fl_rectf(cx + 1, cy + 1, kCellW - 2, kCellH - 2);

	if (selected) {
		fl_color(FL_WHITE);
		fl_rect(cx, cy, kCellW, kCellH);
	} else {
		fl_color(fl_rgb_color(40, 40, 40));
		fl_rect(cx, cy, kCellW, kCellH);
	}

	/* dB text inside cell (only if tall enough) */
	if (kCellH >= 14) {
		char buf[8];
		if (gain == 0)
			snprintf(buf, sizeof(buf), "-inf");
		else if (gain >= HDSP_GAIN_UNITY)
			snprintf(buf, sizeof(buf), "0");
		else {
			float db = 20.0f * log10f((float)gain / HDSP_GAIN_UNITY);
			snprintf(buf, sizeof(buf), "%.0f", db);
		}
		fl_color(gain > 2000 ? FL_BLACK : fl_rgb_color(160, 160, 160));
		fl_font(FL_HELVETICA, 8);
		fl_draw(buf, cx, cy, kCellW, kCellH,
		        FL_ALIGN_CENTER | FL_ALIGN_CLIP);
	}
}

void MatrixWidget::draw() {
	fl_push_clip(x(), y(), w(), h());

	/* Background */
	fl_color(fl_rgb_color(30, 30, 30));
	fl_rectf(x(), y(), w(), h());

	if (nch_ == 0) {
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 14);
		fl_draw("No device connected", x(), y(), w(), h(), FL_ALIGN_CENTER);
		fl_pop_clip();
		return;
	}

	/* --- Column headers (output labels) --- */
	for (int o = 0; o < nch_; ++o) {
		int hx = content_x() + o * kCellW;
		int hy = y();
		if (hx + kCellW < x() || hx > x() + w())
			continue; /* out of viewport */

		fl_color(fl_rgb_color(50, 50, 80));
		fl_rectf(hx + 1, hy + 1, kCellW - 2, kHdrH - 2);
		fl_color(fl_rgb_color(80, 80, 120));
		fl_rect(hx, hy, kCellW, kHdrH);

		char buf[16];
		snprintf(buf, sizeof(buf), "Out %d", o + 1);
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 9);
		fl_draw(buf, hx, hy, kCellW, kHdrH,
		        FL_ALIGN_CENTER | FL_ALIGN_CLIP);
	}

	/* --- Row labels (source names) --- */
	for (int s = 0; s < nsrc_; ++s) {
		int ly = content_y() + s * kCellH;
		if (ly + kCellH < y() || ly > y() + h())
			continue;

		bool is_cap = (s >= nch_);
		int  ch     = is_cap ? s - nch_ : s;

		fl_color(is_cap ? fl_rgb_color(50, 40, 30) : fl_rgb_color(30, 40, 55));
		fl_rectf(x() + 1, ly + 1, kLabelW - 2, kCellH - 2);
		fl_color(fl_rgb_color(70, 70, 70));
		fl_rect(x(), ly, kLabelW, kCellH);

		char buf[32];
		snprintf(buf, sizeof(buf), is_cap ? "In  %2d" : "PB  %2d", ch + 1);
		fl_color(FL_WHITE);
		fl_font(FL_HELVETICA, 9);
		fl_draw(buf, x() + 2, ly, kLabelW - 4, kCellH,
		        FL_ALIGN_LEFT | FL_ALIGN_CLIP);

		/* Section separator between playbacks and captures */
		if (s == nch_ - 1) {
			fl_color(fl_rgb_color(100, 80, 40));
			fl_line(x(), ly + kCellH - 1, x() + w(), ly + kCellH - 1);
		}
	}

	/* --- Matrix cells --- */
	for (int s = 0; s < nsrc_; ++s) {
		int cy = content_y() + s * kCellH;
		if (cy + kCellH < y() || cy > y() + h())
			continue;

		for (int o = 0; o < nch_; ++o) {
			int cx = content_x() + o * kCellW;
			if (cx + kCellW < x() || cx > x() + w())
				continue;

			uint32_t addr  = addr_of(s, o);
			bool     sel   = ((int)addr == sel_addr_);
			draw_cell(s, o, sel);
		}
	}

	/* Corner decoration (top-left label area) */
	fl_color(fl_rgb_color(20, 20, 20));
	fl_rectf(x(), y(), kLabelW, kHdrH);
	fl_color(fl_rgb_color(90, 90, 90));
	fl_font(FL_HELVETICA_BOLD, 9);
	fl_draw("SRC \\ OUT", x(), y(), kLabelW, kHdrH,
	        FL_ALIGN_CENTER | FL_ALIGN_CLIP);

	fl_pop_clip();
}

int MatrixWidget::handle(int event) {
	switch (event) {
	case FL_PUSH:
		if (nch_ == 0)
			return 1;
		{
			int sr, oc;
			if (cell_at_pixel(Fl::event_x(), Fl::event_y(), sr, oc)) {
				uint32_t addr = addr_of(sr, oc);
				sel_addr_ = (int)addr;
				damage(FL_DAMAGE_ALL);
				if (on_select_cb_)
					on_select_cb_(addr, gains_[addr]);
			}
		}
		return 1;

	case FL_MOUSEWHEEL:
		scroll_by(Fl::event_dx() * kCellW,
		          Fl::event_dy() * kCellH * 3);
		return 1;

	case FL_DRAG:
		if (Fl::event_button() == FL_MIDDLE_MOUSE) {
			static int last_x = 0, last_y = 0;
			if (Fl::event_state(FL_BUTTON2)) {
				scroll_by(last_x - Fl::event_x(),
				          last_y - Fl::event_y());
			}
			last_x = Fl::event_x();
			last_y = Fl::event_y();
			return 1;
		}
		break;
	}
	return Fl_Widget::handle(event);
}
