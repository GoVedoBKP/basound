// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <FL/Fl_Widget.H>
#include <vector>
#include <functional>
#include "hdsp_ioctl.h"

/*
 * MatrixWidget — 2D gain matrix rendered as a scrollable heat-map grid.
 *
 * Layout
 * ------
 *   Rows  = sources: playback channels 1..N, then capture channels 1..N
 *   Cols  = output channels 1..N
 *   Cell  = one gain entry; color encodes gain (black=mute, bright=unity)
 *
 * Interaction
 * -----------
 *   Left-click on a cell selects it and invokes on_select_cb_.
 *   Mouse-wheel and middle-drag scroll the view.
 *   The selected cell can be updated externally via set_gain().
 */
class MatrixWidget : public Fl_Widget {
public:
	MatrixWidget(int x, int y, int w, int h);

	/* Set the matrix dimensions and initial gain values.
	 * Call after opening a device; rebuilds internal state. */
	void init(int nch, const uint16_t *matrix);

	/* Update a single gain entry (does not call device). */
	void set_gain(uint32_t addr, uint16_t gain);

	/* Read the current gain for an address. */
	uint16_t get_gain(uint32_t addr) const;

	/* Replace the full matrix (e.g. after a load-config). */
	void set_matrix(const uint16_t *matrix);

	/* Callback invoked when a cell is selected.
	 * Arguments: address, current gain. */
	void on_select(std::function<void(uint32_t, uint16_t)> cb) {
		on_select_cb_ = cb;
	}

	/* Currently selected cell address, or -1 if none. */
	int selected_addr() const { return sel_addr_; }

	void draw()   override;
	int  handle(int event) override;

private:
	int      nch_;       /* number of physical channels */
	int      nsrc_;      /* number of source rows = 2*nch */

	/* Cell pixel dimensions */
	static constexpr int kCellW  = 52;
	static constexpr int kCellH  = 18;
	static constexpr int kLabelW = 90;  /* left label column */
	static constexpr int kHdrH   = 40; /* top header row */

	/* Scroll offsets (pixels from content origin) */
	int scroll_x_;
	int scroll_y_;

	/* Gain mirror (subset of the full 2048-entry matrix). */
	std::vector<uint16_t> gains_; /* indexed by matrix addr */

	int sel_addr_; /* selected cell, -1=none */

	std::function<void(uint32_t, uint16_t)> on_select_cb_;

	/* Address helpers */
	uint32_t addr_of(int src_row, int out_col) const;
	bool cell_at_pixel(int px, int py, int &src_row, int &out_col) const;

	/* Content area origin in widget coordinates */
	int content_x() const { return x() + kLabelW - scroll_x_; }
	int content_y() const { return y() + kHdrH   - scroll_y_; }

	/* Total content size */
	int content_w() const { return nch_  * kCellW; }
	int content_h() const { return nsrc_ * kCellH; }

	/* Max scroll limits */
	int max_scroll_x() const;
	int max_scroll_y() const;

	void scroll_by(int dx, int dy);
	void draw_cell(int src_row, int out_col, bool selected);
	Fl_Color gain_color(uint16_t gain) const;
};
