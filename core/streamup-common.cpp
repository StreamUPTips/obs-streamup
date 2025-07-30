#include "streamup-common.hpp"

namespace StreamUP {

// Global plugin state moved to StreamUP::PluginState class

// Advanced mask filter settings
const char *advanced_mask_settings[ADVANCED_MASKS_SETTINGS_SIZE] = {
	"rectangle_width",
	"rectangle_height", 
	"position_x",
	"position_y",
	"shape_center_x",
	"shape_center_y",
	"rectangle_corner_radius",
	"mask_gradient_position",
	"mask_gradient_width",
	"circle_radius",
	"heart_size",
	"shape_star_outer_radius",
	"shape_star_inner_radius",
	"star_corner_radius",
	"shape_feather_amount"
};

} // namespace StreamUP