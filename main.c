#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <memory.h>
#include <jpeglib.h>
#include "config.h"

const unsigned char INPUT_IMAGE_COMPONENTS_NUMBER = 3;
const unsigned char OUTPUT_IMAGE_COMPONENTS_NUMBER = 1;

const unsigned char calculate_gray(
  const unsigned char red,
  const unsigned char green,
  const unsigned char blue
) {
  return red * 0.2126 + green * 0.7152 + blue * 0.0722;
}

void transform_input_image_row(
  const JSAMPROW input_image_row_sample,
  const JSAMPROW output_image_row_sample,
  const size_t original_width
) {
  for (size_t j = 0; j < original_width; j++) {
    const size_t pixel_index = j * INPUT_IMAGE_COMPONENTS_NUMBER;
    const unsigned char red = input_image_row_sample[pixel_index + 0];
    const unsigned char green = input_image_row_sample[pixel_index + 1];
    const unsigned char blue = input_image_row_sample[pixel_index + 2];
    const unsigned char gray = calculate_gray(red, green, blue);
    output_image_row_sample[j] = gray;
  }
}

void set_decompressor_options(
  struct jpeg_decompress_struct *decompressor,
  struct jpeg_error_mgr *error_manager,
  FILE *input_file
) {
  decompressor->err = jpeg_std_error(error_manager);
  jpeg_create_decompress(decompressor);
  jpeg_stdio_src(decompressor, input_file);
  (void)jpeg_read_header(decompressor, TRUE);
  (void)jpeg_start_decompress(decompressor);
}

void set_compressor_options(
  struct jpeg_compress_struct *compressor,
  const struct jpeg_decompress_struct *decompressor,
  struct jpeg_error_mgr *error_manager,
  FILE *output_file
) {
  compressor->err = jpeg_std_error(error_manager);
  jpeg_create_compress(compressor);
  jpeg_stdio_dest(compressor, output_file);
  compressor->in_color_space = JCS_GRAYSCALE;
  compressor->input_components = OUTPUT_IMAGE_COMPONENTS_NUMBER;
  jpeg_set_defaults(compressor);
  compressor->image_width = decompressor->output_width;
  compressor->image_height = decompressor->image_height;
  compressor->density_unit = decompressor->density_unit;
  compressor->X_density = decompressor->X_density;
  compressor->Y_density = decompressor->Y_density;
  jpeg_start_compress(compressor, TRUE);
}

const size_t calculate_input_image_row_length(
  const struct jpeg_decompress_struct *decompressor
) {
  return decompressor->output_width * decompressor->num_components;
}

int transform_image(const char *input_filename, const char *output_filename) {
  FILE *input_file = fopen(input_filename, "rb");
  if (!input_file) {
    (void)fprintf(
      stderr,
      "🛑🙁 error opening jpeg file '%s': %s 🙁🛑\n",
      input_filename,
      strerror(errno)
    );
    return errno;
  }

  FILE *output_file = fopen(output_filename, "wb");
  if (!output_file) {
    (void)fprintf(
      stderr,
      "🛑🙁 error opening output jpeg file '%s': %s 🙁🛑\n",
      output_filename,
      strerror(errno)
    );
    return errno;
  }

  struct jpeg_error_mgr error_manager;

  struct jpeg_decompress_struct decompressor;
  set_decompressor_options(&decompressor, &error_manager, input_file);

  struct jpeg_compress_struct compressor;
  set_compressor_options(&compressor, &decompressor, &error_manager, output_file);

  const size_t input_image_row_length = calculate_input_image_row_length(&decompressor);
  const unsigned int output_image_row_length = compressor.image_width;

  unsigned char *all_buffer = malloc(
    decompressor.image_height * input_image_row_length +
    compressor.image_height * output_image_row_length
  );
  if (all_buffer == NULL) {
    (void)fprintf(stderr, "failed to allocate enough memory.\n");
    exit(-1);
  }

  unsigned char *input_buffer = &all_buffer[0];

  JSAMPROW scan_rows_buffer[decompressor.image_height];
  for (size_t i = 0; i < decompressor.image_height; i++) {
    scan_rows_buffer[i] = &input_buffer[i * input_image_row_length];
  }

  while (decompressor.output_scanline < decompressor.output_height) {
    (void)jpeg_read_scanlines(
      &decompressor,
      &scan_rows_buffer[decompressor.output_scanline],
      decompressor.output_height - decompressor.output_scanline
    );
  }

  unsigned char *output_buffer = &all_buffer[decompressor.image_height * input_image_row_length];
  JSAMPROW output_rows_buffer[compressor.image_height];
  for (size_t i = 0; i < compressor.image_height; i++) {
    output_rows_buffer[i] = &output_buffer[i * output_image_row_length];
  }

  for (size_t i = 0; i < decompressor.image_height; i++) {
    transform_input_image_row(
      scan_rows_buffer[i],
      output_rows_buffer[i],
      decompressor.output_width
    );
  }

  for (size_t i = 0; i < compressor.image_height; i++) {
    (void)jpeg_write_scanlines(&compressor, &output_rows_buffer[i], 1);
  }

  (void)jpeg_finish_decompress(&decompressor);
  jpeg_finish_compress(&compressor);
  jpeg_destroy_decompress(&decompressor);
  jpeg_destroy_compress(&compressor);
  free(all_buffer);
  (void)fclose(input_file);
  (void)fclose(output_file);

  return 0;
}

int main() {
  return transform_image(INPUT_IMAGE_FILENAME, OUTPUT_IMAGE_FILENAME);
}
