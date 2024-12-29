#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
  char *discharging_filename =
      "/home/langsjo/nixos/dotfiles/apps/discharging_timeleft.txt";
  int update_interval = 5;
  int window_size_minutes = 10;
  int lines_in_window = window_size_minutes * 60 / update_interval;

  float max_weight = window_size_minutes / 4.0f;
  float weight_decrement = (max_weight - 1) / lines_in_window;

  FILE *upower_stream =
      popen("upower -d | awk '/state:/ {printf \"%s\", $2;exit}'", "r");
  if (!upower_stream) {
    printf("fail line 19\n");
    return EXIT_FAILURE;
  }
  char *state = malloc(20);
  fscanf(upower_stream, "%s", state);
  pclose(upower_stream);

  if (!strcmp(state, "discharging")) {
    FILE *discharge_file = fopen(discharging_filename, "r");
    if (!discharge_file) {
      printf("fail line 27\n");
      return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t size;
    float sum = 0;
    int lines_read = 0;

    float times[lines_in_window - 1];

    while (lines_read < lines_in_window - 1 &&
           getline(&line, &size, discharge_file) != -1) {
      float time;
      if (sscanf(line, "%f", &time) < 1) {
        printf("failed line 39\n");
        return EXIT_FAILURE;
      }

      times[lines_read] = time;

      float weighted_time =
          time * (max_weight - weight_decrement * (lines_read + 1));
      sum += weighted_time;

      ++lines_read;
    }
    fclose(discharge_file);
    free(line);

    // sum of arithmetic sequence:
    float total_weight =
        (lines_read + 1) / 2.0f *
        (max_weight - weight_decrement * (lines_read) + max_weight);

    upower_stream = popen(
        "upower -d | awk '/time to empty:/ {printf \"%f %s\", $4, $5;exit}'",
        "r");
    if (!upower_stream)
      return EXIT_FAILURE;

    float time;
    char unit;

    if (fscanf(upower_stream, "%f %c", &time, &unit) < 2) {
      printf("wait...");
      return EXIT_FAILURE;
    }
    pclose(upower_stream);

    discharge_file = fopen(discharging_filename, "w");
    if (!discharge_file) {
      printf("fail line 61\n");
      return EXIT_FAILURE;
    }

    switch (unit) {
    case 'h':
      fprintf(discharge_file, "%f\n", time * 60);
      sum += time * 60 * max_weight;
      break;
    case 'm':
      fprintf(discharge_file, "%f\n", time);
      sum += time * max_weight;
      break;
    }

    for (int i = 0; i < lines_read; ++i) {
      fprintf(discharge_file, "%f\n", times[i]);
    }
    fclose(discharge_file);

    float avg = sum / (total_weight);
    if (avg < 60) {
      printf("%.1f m", avg);
    } else {
      printf("%.2f h", avg / 60);
    }
  } else if (!strcmp(state, "charging")) {
    // clear file content since now battery maybe full
    fclose(fopen(discharging_filename, "w"));

    upower_stream = popen(
        "upower -d | awk '/time to full:/ {printf \"%f %s\", $4, $5;exit}'",
        "r");
    if (!upower_stream)
      return EXIT_FAILURE;
    float time;
    char unit;
    if (fscanf(upower_stream, "%f %c", &time, &unit) < 2) {
      printf("wait...");
      return EXIT_FAILURE;
    }
    pclose(upower_stream);

    switch (unit) {
    case 'h':
      printf("%.2f %c", time, unit);
      break;
    case 'm':
      printf("%.1f %c", time, unit);
      break;
    }
  } else {
    printf("wait...");
  }

  free(state);

  return EXIT_SUCCESS;
}
