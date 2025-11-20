/* Author: Peter Sovietov */

#ifndef AYUMI_H
#define AYUMI_H

enum {
  TONE_CHANNELS = 3,
  DECIMATE_FACTOR = 8,
  FIR_SIZE = 192,
  DC_FILTER_SIZE = 1024
};

enum ayumi_output_mode {
  AYUMI_MONO = 0,
  AYUMI_STEREO = 1,
  AYUMI_THREE_CHANNEL = 2
};

struct tone_channel {
  int tone_period;
  int tone_counter;
  int tone;
  int t_off;
  int n_off;
  int e_on;
  int volume;
  double pan_left;
  double pan_right;
};

struct interpolator {
  double c[4];
  double y[4];
};

struct dc_filter {
  double sum;
  double delay[DC_FILTER_SIZE];
};

struct ayumi_output {
  struct interpolator interpolator;
  double fir[FIR_SIZE * 2];
  struct dc_filter dc;
  double value;
};

struct ayumi {
  struct tone_channel channels[TONE_CHANNELS];
  int noise_period;
  int noise_counter;
  int noise;
  int envelope_counter;
  int envelope_period;
  int envelope_shape;
  int envelope_segment;
  int envelope;
  const double* dac_table;
  double step;
  double x;
  int fir_index;
  int dc_index;
  enum ayumi_output_mode output_mode;
  struct ayumi_output outputs[TONE_CHANNELS];
  double channel_out[TONE_CHANNELS];
};

int ayumi_configure(struct ayumi* ay, int is_ym, double clock_rate, int sr);
void ayumi_set_pan(struct ayumi* ay, int index, double pan, int is_eqp);
void ayumi_set_tone(struct ayumi* ay, int index, int period);
void ayumi_set_noise(struct ayumi* ay, int period);
void ayumi_set_mixer(struct ayumi* ay, int index, int t_off, int n_off, int e_on);
void ayumi_set_volume(struct ayumi* ay, int index, int volume);
void ayumi_set_envelope(struct ayumi* ay, int period);
void ayumi_set_envelope_shape(struct ayumi* ay, int shape);
void ayumi_set_output_mode(struct ayumi* ay, enum ayumi_output_mode mode);
void ayumi_process(struct ayumi* ay);
void ayumi_remove_dc(struct ayumi* ay);
double ayumi_get_output(struct ayumi* ay, int output_index);

#endif
