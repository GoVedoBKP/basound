// SPDX-License-Identifier: GPL-3.0-or-later
#include "hdsp_device.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <cstdio>

HdspDevice::HdspDevice() : fd_(-1) {
	memset(&cfg_, 0, sizeof(cfg_));
}

HdspDevice::~HdspDevice() {
	close();
}

bool HdspDevice::open(const char *path) {
	close();

	fd_ = ::open(path, O_RDWR);
	if (fd_ < 0)
		return false;

	if (::ioctl(fd_, HDSP_IOCTL_GET_CONFIG, &cfg_) < 0) {
		::close(fd_);
		fd_ = -1;
		return false;
	}

	path_ = path;
	return true;
}

void HdspDevice::close() {
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}
	path_.clear();
	memset(&cfg_, 0, sizeof(cfg_));
}

bool HdspDevice::get_mixer(struct hdsp_mixer_ioctl &out) {
	return fd_ >= 0 && ::ioctl(fd_, HDSP_IOCTL_GET_MIXER, &out) == 0;
}

bool HdspDevice::set_entry(uint32_t addr, uint16_t gain) {
	if (fd_ < 0)
		return false;
	struct hdsp_mixer_entry e;
	e.addr = addr;
	e.gain = gain;
	e._pad = 0;
	return ::ioctl(fd_, HDSP_IOCTL_SET_ENTRY, &e) == 0;
}

bool HdspDevice::set_mixer(const struct hdsp_mixer_ioctl &mx) {
	return fd_ >= 0 && ::ioctl(fd_, HDSP_IOCTL_SET_MIXER,
	    const_cast<struct hdsp_mixer_ioctl *>(&mx)) == 0;
}

bool HdspDevice::get_levels(struct hdsp_peak_levels &out) {
	return fd_ >= 0 && ::ioctl(fd_, HDSP_IOCTL_GET_LEVELS, &out) == 0;
}

std::vector<std::string> HdspDevice::scan() {
	std::vector<std::string> result;
	for (int i = 0; i < 8; ++i) {
		char path[32];
		snprintf(path, sizeof(path), "/dev/hdsp%d", i);
		int fd = ::open(path, O_RDWR);
		if (fd < 0)
			continue;
		struct hdsp_config_info cfg;
		if (::ioctl(fd, HDSP_IOCTL_GET_CONFIG, &cfg) == 0)
			result.push_back(path);
		::close(fd);
	}
	return result;
}

float HdspDevice::peak_to_db(uint32_t peak) {
	if (peak == 0)
		return -144.0f;
	return 20.0f * log10f((float)peak / 2147483647.0f);
}

float HdspDevice::rms_to_db(uint64_t rms) {
	if (rms == 0)
		return -144.0f;
	/* Hardware RMS is a squared accumulator; treat as power. */
	return 10.0f * log10f((double)rms / (double)(1ULL << 63));
}
