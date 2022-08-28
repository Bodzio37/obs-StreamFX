#include "dnxhd_handler.hpp"
#include <array>
#include "../codecs/dnxhr.hpp"
#include "ffmpeg/tools.hpp"
#include "plugin.hpp"

extern "C" {
#include <obs-module.h>
}

using namespace streamfx::encoder::ffmpeg::handler;
using namespace streamfx::encoder::codec::dnxhr;

void dnxhd_handler::adjust_info(ffmpeg_factory* fac, const AVCodec*, std::string&, std::string& name, std::string&)
{
	//Most people don't know what VC3 is and only know it as DNx.
	//Change name to make it easier to find.
	name = "Avid DNxHR (via FFmpeg)";
}

void dnxhd_handler::override_colorformat(AVPixelFormat& target_format, obs_data_t* settings, const AVCodec* codec,
										 AVCodecContext*)
{
	static const std::array<std::pair<const char*, AVPixelFormat>, static_cast<size_t>(5)> profile_to_format_map{
		std::pair{"dnxhr_lb", AV_PIX_FMT_YUV422P}, std::pair{"dnxhr_sq", AV_PIX_FMT_YUV422P},
		std::pair{"dnxhr_hq", AV_PIX_FMT_YUV422P}, std::pair{"dnxhr_hqx", AV_PIX_FMT_YUV422P10LE},
		std::pair{"dnxhr_444", AV_PIX_FMT_YUV444P10LE}};

	const char* selected_profile = obs_data_get_string(settings, S_CODEC_DNXHR_PROFILE);
	for (const auto& kv : profile_to_format_map) {
		if (strcmp(kv.first, selected_profile) == 0) {
			target_format = kv.second;
			return;
		}
	}

	//Fallback for (yet) unknown formats
	target_format = AV_PIX_FMT_YUV422P;
}

void dnxhd_handler::get_defaults(obs_data_t* settings, const AVCodec*, AVCodecContext*, bool)
{
	obs_data_set_default_string(settings, S_CODEC_DNXHR_PROFILE, "dnxhr_sq");
}

bool dnxhd_handler::has_keyframe_support(ffmpeg_factory* instance)
{
	return false;
}

bool dnxhd_handler::has_pixel_format_support(ffmpeg_factory* instance)
{
	return false;
}

inline const char* dnx_profile_to_display_name(const char* profile)
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "%s.%s", S_CODEC_DNXHR_PROFILE, profile);
	return D_TRANSLATE(buffer);
}

void dnxhd_handler::get_properties(obs_properties_t* props, const AVCodec* codec, AVCodecContext* context, bool)
{
	AVCodecContext* ctx = context;

	//Create dummy context if null was passed to the function
	if (!ctx) {
		ctx = avcodec_alloc_context3(codec);
		if (!ctx->priv_data) {
			avcodec_free_context(&ctx);
			return;
		}
	}
	auto p = obs_properties_add_list(props, S_CODEC_DNXHR_PROFILE, D_TRANSLATE(S_CODEC_DNXHR_PROFILE),
									 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	streamfx::ffmpeg::tools::avoption_list_add_entries(ctx->priv_data, "profile", [&p](const AVOption* opt) {
		if (strcmp(opt->name, "dnxhd") == 0) {
			//Do not show DNxHD profile as it is outdated and should not be used.
			//It's also very picky about framerate and framesize combos, which makes it even less useful
			return;
		}

		//ffmpeg returns the profiles for DNxHR from highest to lowest.
		//Lowest to highest is what people usually expect.
		//Therefore, new entries will always be inserted at the top, effectively reversing the list
		obs_property_list_insert_string(p, 0, dnx_profile_to_display_name(opt->name), opt->name);
	});

	//Free context if we created it here
	if (ctx && ctx != context) {
		avcodec_free_context(&ctx);
	}
}

void dnxhd_handler::update(obs_data_t* settings, const AVCodec* codec, AVCodecContext* context)
{
	const char* profile = obs_data_get_string(settings, S_CODEC_DNXHR_PROFILE);
	av_opt_set(context, "profile", profile, AV_OPT_SEARCH_CHILDREN);
}

void dnxhd_handler::log_options(obs_data_t* settings, const AVCodec* codec, AVCodecContext* context)
{
	DLOG_INFO("[%s]   Avid DNxHR:", codec->name);
	streamfx::ffmpeg::tools::print_av_option_string2(context, "profile", "    Profile",
													 [](int64_t v, std::string_view o) { return std::string(o); });
}
