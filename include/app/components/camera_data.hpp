#ifndef CAMERA_DATA_HPP
#define CAMERA_DATA_HPP

#include <random>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for Camera component
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for CameraComponent functionality. Generates realistic pixel matrix
 * data for image simulation. Initially simplified to handle EXTERNAL_PIXEL_MATRIX
 * only, but can be extended to support RGB_IMAGE, VIDEO_STREAM, and CAMERA_METADATA.
 * 
 * Replaces inheritance-based data storage with pure composition.
 */
struct CameraData : public ComponentData {
    // Producer data: random number generation for pixel matrix data
    std::random_device rd;
    std::mt19937 gen;
    
    // Image dimensions
    int image_width;
    int image_height;
    int bytes_per_pixel;
    
    // Pixel value distributions
    std::uniform_int_distribution<> pixel_dist;     // Pixel intensity values (0-255)
    std::uniform_int_distribution<> noise_dist;     // Noise variation
    
    // Image pattern distributions (for realistic image simulation)
    std::uniform_real_distribution<> pattern_dist;  // Pattern variation (0.0-1.0)
    std::uniform_int_distribution<> frame_dist;     // Frame variation
    
    // Timing distribution
    std::uniform_int_distribution<> delay_dist;     // Delay between frames (ms)
    
    /**
     * @brief Constructor initializes random number generation with realistic image parameters
     * 
     * Sets up the same random distributions as the original CameraComponent:
     * - Image dimensions: 640x480 (VGA resolution, typical for automotive cameras)
     * - Bytes per pixel: 1 (grayscale) initially, can be extended to 3 (RGB)
     * - Pixel values: 0-255 (standard 8-bit grayscale)
     * - Noise: ±10 (realistic sensor noise)
     * - Pattern variation: 0.0-1.0 (for generating realistic image patterns)
     * - Frame variation: 0-100 (frame-to-frame variation)
     * - Delay: 30-40ms (typical camera ~30Hz frame rate)
     */
    CameraData() : gen(rd()),
                   image_width(640),                    // VGA width (pixels)
                   image_height(480),                   // VGA height (pixels)
                   bytes_per_pixel(1),                  // Grayscale initially (can extend to 3 for RGB)
                   pixel_dist(0, 255),                  // Pixel intensity (0-255)
                   noise_dist(-10, 10),                 // Noise variation (±10)
                   pattern_dist(0.0, 1.0),              // Pattern variation (0.0-1.0)
                   frame_dist(0, 100),                  // Frame variation (0-100)
                   delay_dist(30, 40) {}                // Milliseconds delay (camera typically ~30Hz)
    
    /**
     * @brief Update the image dimensions
     * 
     * Allows dynamic reconfiguration of the image size.
     * Useful for testing different resolutions or runtime configuration.
     * 
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param bpp Bytes per pixel (1 for grayscale, 3 for RGB)
     */
    void update_image_dimensions(int width, int height, int bpp = 1) {
        if (width > 0 && height > 0 && bpp > 0) {
            image_width = width;
            image_height = height;
            bytes_per_pixel = bpp;
        }
    }
    
    /**
     * @brief Update the pixel value generation ranges
     * 
     * @param min_pixel Minimum pixel value
     * @param max_pixel Maximum pixel value
     * @param noise_range Noise variation range (±noise_range)
     */
    void update_pixel_range(int min_pixel, int max_pixel, int noise_range) {
        if (min_pixel >= 0 && max_pixel > min_pixel && noise_range >= 0) {
            pixel_dist = std::uniform_int_distribution<>(min_pixel, max_pixel);
            noise_dist = std::uniform_int_distribution<>(-noise_range, noise_range);
        }
    }
    
    /**
     * @brief Update the frame timing parameters
     * 
     * @param min_delay_ms Minimum delay between frames (milliseconds)
     * @param max_delay_ms Maximum delay between frames (milliseconds)
     */
    void update_timing_range(int min_delay_ms, int max_delay_ms) {
        if (min_delay_ms > 0 && max_delay_ms > min_delay_ms) {
            delay_dist = std::uniform_int_distribution<>(min_delay_ms, max_delay_ms);
        }
    }
    
    /**
     * @brief Get the total image size in bytes
     * 
     * @return Total size of one image frame in bytes
     */
    std::size_t get_image_size() const {
        return image_width * image_height * bytes_per_pixel;
    }
};

#endif // CAMERA_DATA_HPP 