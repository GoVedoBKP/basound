// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include "hdsp_ioctl.h"

/*
 * HdspDevice — thin RAII wrapper around /dev/hdspN.
 *
 * All ioctl calls return true on success, false on error (errno preserved).
 * The device is automatically closed in the destructor.
 */
class HdspDevice {
public:
	HdspDevice();
	~HdspDevice();

	/* Open a specific device node (e.g. "/dev/hdsp0").
	 * Reads and caches the card config on success. */
	bool open(const char *path);
	void close();
	bool is_open() const { return fd_ >= 0; }

	/* Ioctl wrappers */
	bool get_mixer(struct hdsp_mixer_ioctl &out);
	bool set_entry(uint32_t addr, uint16_t gain);
	bool set_mixer(const struct hdsp_mixer_ioctl &mx);
	bool get_levels(struct hdsp_peak_levels &out);

	/* Cached config (valid after open()) */
	const struct hdsp_config_info &config() const { return cfg_; }
	const char *path() const { return path_.c_str(); }

	/* Scan /dev/hdsp0 .. /dev/hdsp7 and return all responsive paths. */
	static std::vector<std::string> scan();

	/* Convert a 32-bit hardware peak value to dBFS.
	 * Returns -144.0f for silence (value == 0). */
	static float peak_to_db(uint32_t peak);

	/* Convert a 64-bit hardware RMS accumulator to dBFS. */
	static float rms_to_db(uint64_t rms);

private:
	int fd_;
	std::string path_;
	struct hdsp_config_info cfg_;
};
