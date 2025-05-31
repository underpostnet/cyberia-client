import os

os.environ["XLA_FLAGS"] = "--xla_gpu_cuda_data_dir=/root/.conda/envs/cuda_env"

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np
import json
import matplotlib.pyplot as plt

# --- Configuration for Loading ---
# Directory where the trained model and color map are saved
MODEL_LOAD_DIR = "trained_pixel_art_model"
COLOR_MAP_FILE = os.path.join(MODEL_LOAD_DIR, "color_map.json")
# Updated DECODER_MODEL_PATH to include the .keras extension
DECODER_MODEL_PATH = os.path.join(MODEL_LOAD_DIR, "decoder_model.keras")

# Latent dimension must match the one used during training
LATENT_DIM = 128  # Ensure this matches the LATENT_DIM from your training script


# --- Define the Decoder's architecture (must match the saved model) ---
# We need to define the decoder's structure again to load it correctly.
# This part should be identical to the build_decoder function in your training script.
def build_decoder(latent_dim, output_shape=(25, 25, 1)):  # Default output_shape
    """Builds the decoder part of the VAE."""
    decoder_inputs = keras.Input(shape=(latent_dim,))
    initial_dense_dim = 7 * 7 * 64
    x = layers.Dense(initial_dense_dim, activation="relu")(decoder_inputs)
    x = layers.Reshape((7, 7, 64))(x)

    x = layers.Conv2DTranspose(64, 3, activation="relu", strides=2, padding="same")(x)
    x = layers.Conv2DTranspose(32, 3, activation="relu", strides=2, padding="same")(x)
    decoder_outputs = layers.Conv2DTranspose(
        1, 3, activation="sigmoid", padding="same"
    )(x)

    # Re-apply cropping logic as in the training script
    crop_height = decoder_outputs.shape[1] - output_shape[0]
    crop_width = decoder_outputs.shape[2] - output_shape[1]

    if crop_height > 0 or crop_width > 0:
        top_crop = crop_height // 2
        bottom_crop = crop_height - top_crop
        left_crop = crop_width // 2
        right_crop = crop_width - left_crop
        decoder_outputs = layers.Cropping2D(
            cropping=((top_crop, bottom_crop), (left_crop, right_crop))
        )(decoder_outputs)

    return keras.Model(decoder_inputs, decoder_outputs, name="decoder")


# --- Function to Generate and Visualize ---
def generate_and_visualize_skin(vae_decoder, latent_dim, color_map, num_samples=1):
    """Generates new pixel art frames and visualizes them using the provided color map."""
    random_latent_vectors = tf.random.normal(shape=(num_samples, latent_dim))
    generated_frames_normalized = vae_decoder.predict(random_latent_vectors)

    print(
        f"\nGenerated Normalized Frames (Raw Sigmoid Output) Min: {np.min(generated_frames_normalized):.4f}, Max: {np.max(generated_frames_normalized):.4f}"
    )

    max_color_index = len(color_map) - 1
    generated_frames_denormalized_before_clip = (
        generated_frames_normalized * max_color_index
    )
    generated_frames_indices = np.round(generated_frames_denormalized_before_clip)
    generated_frames_indices = np.clip(
        generated_frames_indices, 0, max_color_index
    ).astype(int)

    print(
        f"Generated Indices (Before Round & Clip) Min: {np.min(generated_frames_denormalized_before_clip):.2f}, Max: {np.max(generated_frames_denormalized_before_clip):.2f}"
    )
    print(
        f"Generated Indices (After Round & Clip) Min: {np.min(generated_frames_indices):.2f}, Max: {np.max(generated_frames_indices):.2f}"
    )

    print(f"\nGenerated {num_samples} new pixel art frames:")
    plt.figure(figsize=(num_samples * 3, 4))

    for i, skin_frame_indices in enumerate(generated_frames_indices):
        skin_frame_2d_indices = skin_frame_indices.squeeze()
        skin_frame_rgba = color_map[skin_frame_2d_indices]

        plt.subplot(1, num_samples, i + 1)
        plt.imshow(skin_frame_rgba)
        plt.title(f"Generated {i+1}")
        plt.axis("off")

        print(f"\n--- Generated Skin Frame {i+1} (Indices) ---")
        for row in skin_frame_2d_indices:
            print(row.tolist())

    plt.tight_layout()
    plt.show()


# --- Main execution ---
if __name__ == "__main__":
    # Ensure the model directory exists
    if not os.path.exists(MODEL_LOAD_DIR):
        print(f"Error: Model directory '{MODEL_LOAD_DIR}' not found.")
        print(
            "Please run 'object_layer_training.py' first to train and save the model."
        )
    else:
        try:
            # Load the color map
            with open(COLOR_MAP_FILE, "r") as f:
                loaded_color_map_list = json.load(f)
            loaded_color_map_rgba = np.array(loaded_color_map_list, dtype=np.float32)

            # Load the decoder model using keras.models.load_model for .keras format
            # We need to provide custom objects if any custom layers were used in the model
            # In this case, 'Sampling' is a custom layer used in the encoder, not directly in the decoder.
            # However, if the decoder itself had custom layers, they would need to be passed here.
            # Since build_decoder creates the model structure, we can load weights directly.
            loaded_decoder = build_decoder(LATENT_DIM)
            # When saving with .keras, load_weights expects the path to the .keras file directly.
            loaded_decoder.load_weights(DECODER_MODEL_PATH)

            print(f"Model loaded successfully from {DECODER_MODEL_PATH}")
            print(f"Color map loaded successfully from {COLOR_MAP_FILE}")
            print(f"Loaded Color Map (Normalized RGBA):")
            print(loaded_color_map_rgba)

            # Generate and visualize new skins
            generate_and_visualize_skin(
                loaded_decoder, LATENT_DIM, loaded_color_map_rgba, num_samples=5
            )

        except Exception as e:
            print(f"An error occurred during loading or generation: {e}")
            print(
                "Please ensure the model and color map files exist and are not corrupted."
            )
            print(f"Expected model path: {DECODER_MODEL_PATH}")
            print(f"Expected color map path: {COLOR_MAP_FILE}")
