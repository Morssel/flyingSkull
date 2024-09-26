// vim: set path+=/llama.cpp:/llama.cpp/ggml/include:
#include "llama.cpp/ggml/include/ggml.h"
#include "llama.cpp/include/llama.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

int main(int argc, char **argv) {
  const char *model_path =
      "path_to_your_model.bin"; // Replace with your model path

  // Initialize the llama backend
  llama_backend_init();

  // Load the model
  struct llama_model_params model_params = llama_model_default_params();
  struct llama_model *model =
      llama_load_model_from_file(model_path, model_params);
  if (model == NULL) {
    fprintf(stderr, "Failed to load model\n");
    return 1;
  }

  // Create the context
  struct llama_context_params ctx_params = llama_context_default_params();
  struct llama_context *ctx = llama_new_context_with_model(model, ctx_params);
  if (ctx == NULL) {
    fprintf(stderr, "Failed to create context\n");
    llama_free_model(model);
    return 1;
  }

  // Get the prompt from the user
  const char *prompt = "Hello, how are you today?";
  if (argc > 1) {
    prompt = argv[1];
  }
  printf("Prompt: %s\n", prompt);

  int32_t prompt_len = strlen(prompt);

  int32_t max_tokens = 512;
  llama_token tokens[max_tokens];

  int32_t n_tokens = llama_tokenize(model, prompt, prompt_len, tokens,
                                    max_tokens, true, false);
  if (n_tokens < 0) {
    fprintf(stderr, "Failed to tokenize prompt\n");
    llama_free(ctx);
    llama_free_model(model);
    return 1;
  }

  // Process the prompt tokens and request logits for the last token
  int8_t logits_array[n_tokens];
  memset(logits_array, 0, sizeof(logits_array));
  logits_array[n_tokens - 1] = 1; // Request logits for the last token

  struct llama_batch batch;
  batch.n_tokens = n_tokens;
  batch.token = tokens;
  batch.embd = NULL;
  batch.pos = NULL;
  batch.n_seq_id = NULL;
  batch.seq_id = NULL;
  batch.logits = logits_array;
  batch.all_pos_0 = 0;
  batch.all_pos_1 = 1;
  batch.all_seq_id = 0;

  int32_t res = llama_decode(ctx, batch);
  if (res != 0) {
    fprintf(stderr, "Failed to process prompt\n");
    llama_free(ctx);
    llama_free_model(model);
    return 1;
  }

  // Initialize the sampler
  struct llama_sampler_chain_params sparams =
      llama_sampler_chain_default_params();
  struct llama_sampler *smpl = llama_sampler_chain_init(sparams);

  // Add sampling methods (you can adjust these parameters)
  llama_sampler_chain_add(
      smpl, llama_sampler_init_top_k(40)); // Top-k sampling with k=40
  llama_sampler_chain_add(
      smpl, llama_sampler_init_temp(0.7)); // Temperature sampling with t=0.7
  llama_sampler_chain_add(
      smpl, llama_sampler_init_repeat_penalty(1.1)); // Repeat penalty
  llama_sampler_chain_add(
      smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED)); // Final sampler

  // Generate tokens
  int max_generation_tokens = 100;
  llama_pos pos = n_tokens;
  llama_seq_id seq_id = 0;

  for (int i = 0; i < max_generation_tokens; ++i) {
    // Sample the next token
    llama_token next_token = llama_sampler_sample(smpl, ctx, -1);

    // Accept the token
    llama_sampler_accept(smpl, next_token);

    // Check for end of text
    if (next_token == llama_token_eos(model)) {
      printf("\n<end of text>\n");
      break;
    }

    // Convert the token to text
    char token_text[8];
    int32_t len = llama_token_to_piece(model, next_token, token_text,
                                       sizeof(token_text), 0, false);
    if (len > 0) {
      printf("%.*s", len, token_text);
      fflush(stdout);
    }

    // Create a batch with the new token
    llama_token token_array[1] = {next_token};
    llama_pos pos_array[1] = {pos};
    int8_t logits_array[1] = {1}; // Request logits for this token

    struct llama_batch batch_next;
    batch_next.n_tokens = 1;
    batch_next.token = token_array;
    batch_next.embd = NULL;
    batch_next.pos = pos_array;
    batch_next.n_seq_id = NULL;
    batch_next.seq_id = NULL;
    batch_next.logits = logits_array;

    // Call llama_decode with the new batch
    res = llama_decode(ctx, batch_next);
    if (res != 0) {
      fprintf(stderr, "Failed to decode token\n");
      break;
    }

    // Increment position
    pos++;
  }

  printf("\n");

  // Clean up
  llama_sampler_free(smpl);
  llama_free(ctx);
  llama_free_model(model);

  return 0;
}
