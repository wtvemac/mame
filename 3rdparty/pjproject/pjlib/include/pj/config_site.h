// license: Unlicense / public domain
// copyright-holders: wtvemac

// Description here

#ifndef __CONFIG_SITE_H__
#define __CONFIG_SITE_H__

// L16 8kHz is the best possible codec for MoIP but it's heavier on bandwidth
// It's essentally raw 8kHz PCM over IP. Should be able to do V90 over this.
// The extra bandwidth is nothing over most modern broadband connections
// e.g. if you can watch YouTube @ 1080p+ then this is nothing
#define PJMEDIA_HAS_L16_CODEC            1
#define PJMEDIA_CODEC_L16_HAS_8KHZ_MONO  1

// G.711 is a good (and well-supported) fallback codec we can use for MoIP.
// compressed logarithmicly (companding to 8-bit which runs @ 8kHz or 64kbps)
// The uLaw version of this is preferred on WebTV Redialed / dialup.world
// This is a fallback because of quantization errors from logarithmic companding
#define PJMEDIA_HAS_G711_CODEC      1
// aLaw/uLaw table is for faster performance
#define PJMEDIA_HAS_ALAW_ULAW_TABLE 1

// Any lossy voice codec should be avoided for MoIP
// On paper these codecs look better but the compression destroys modem modulation
#define PJMEDIA_HAS_SPEEX_CODEC 0
#define PJMEDIA_HAS_ILBC_CODEC  0
#define PJMEDIA_HAS_GSM_CODEC   0
#define PJMEDIA_HAS_G722_CODEC  0
#define PJMEDIA_HAS_G7221_CODEC 0
#define PJMEDIA_HAS_OPUS_CODEC  0

// This is just for audio, no need to lump in video
#define PJMEDIA_HAS_VIDEO 0

// Any tomfoolery on the audio needs to be disabled. We need the audio unmolested.
// All of this is good for VoIP but bad for MoIP

// Turn off automatic gain control
#define PJMEDIA_CONF_USE_AGC 0

// Disable "Waveform-based Similarity Overlap and Add"
// used for correcting voice over bad connections
#define PJMEDIA_WSOLA_IMP PJMEDIA_WSOLA_IMP_NULL

// Reduce CPU usage since MoIP doesn't require any call mixing for a conference bridge
#define PJMEDIA_CONF_USE_SWITCH_BOARD 1

// MAME build: pjproject is compiled by genie (scripts/src/3rdparty.lua) without running
// ./configure, so PJ_AUTOCONF is never set and pj/config.h picks the static per-OS header
// (os_linux.h, os_darwinos.h, os_win32.h). The settings below cover what ./configure used
// to decide ("--disable-video --disable-libwebrtc --disable-libsrtp --disable-resample
// --disable-sound") and keep the build free of external libraries.

// No SRTP, TLS or OpenSSL; MoIP runs plain SIP/RTP over UDP
#define PJMEDIA_HAS_SRTP 0
#define PJ_HAS_SSL_SOCK  0

// No resampling; the conference bridge, sound clock and codec all run at the MoIP rate
#define PJMEDIA_RESAMPLE_IMP PJMEDIA_RESAMPLE_NONE

// No echo canceller backends (they live in third_party/); ecTailLen is 0 anyway
#define PJMEDIA_HAS_SPEEX_AEC  0
#define PJMEDIA_HAS_WEBRTC_AEC 0

// No sound devices; the bridge uses pjsua's null device and its own media port
#define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO 0
#define PJMEDIA_AUDIO_DEV_HAS_WMME      0

// POSIX socket features ./configure detects but os_linux.h/os_darwinos.h leave out
#if defined(__linux__) || defined(__APPLE__)
#ifndef PJ_HAS_NETINET_TCP_H
#define PJ_HAS_NETINET_TCP_H 1
#endif
#ifndef PJ_HAS_NET_IF_H
#define PJ_HAS_NET_IF_H 1
#endif
#ifndef PJ_HAS_IFADDRS_H
#define PJ_HAS_IFADDRS_H 1
#endif
#ifndef PJ_HAS_LIMITS_H
#define PJ_HAS_LIMITS_H 1
#endif
#ifndef PJ_HAS_STDINT_H
#define PJ_HAS_STDINT_H 1
#endif
#ifndef PJ_HAS_INTTYPES_H
#define PJ_HAS_INTTYPES_H 1
#endif
#ifndef PJ_HAS_FCNTL_H
#define PJ_HAS_FCNTL_H 1
#endif
#ifndef PJ_SOCK_HAS_INET_PTON
#define PJ_SOCK_HAS_INET_PTON 1
#endif
#ifndef PJ_SOCK_HAS_INET_NTOP
#define PJ_SOCK_HAS_INET_NTOP 1
#endif
#ifndef PJ_SOCK_HAS_GETADDRINFO
#define PJ_SOCK_HAS_GETADDRINFO 1
#endif
#ifndef PJ_SOCK_HAS_IPV6_V6ONLY
#define PJ_SOCK_HAS_IPV6_V6ONLY 1
#endif
#ifndef PJ_SOCK_HAS_SOCKETPAIR
#define PJ_SOCK_HAS_SOCKETPAIR 1
#endif
#endif

#endif // __CONFIG_SITE_H__
