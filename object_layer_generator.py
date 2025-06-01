import os

# Set XLA_FLAGS to point to the CUDA data directory where libdevice is located.
# This must be set BEFORE importing tensorflow.
os.environ["XLA_FLAGS"] = "--xla_gpu_cuda_data_dir=/root/.conda/envs/cuda_env"

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np
import json
import matplotlib.pyplot as plt

# --- Check for GPU devices ---
gpus = tf.config.list_physical_devices("GPU")
if gpus:
    try:
        # Restrict TensorFlow to only use the first GPU
        tf.config.set_visible_devices(gpus[0], "GPU")
        logical_gpus = tf.config.list_logical_devices("GPU")
        print(
            f"TensorFlow detected {len(gpus)} Physical GPUs, {len(logical_gpus)} Logical GPUs."
        )
        print(f"Using GPU: {gpus[0].name}")
    except RuntimeError as e:
        # Visible devices must be set before GPUs have been initialized
        print(e)
else:
    print("TensorFlow did not detect any GPU devices. Running on CPU.")

# --- Configuration for Loading ---
# Directory where the trained model and color map are saved
MODEL_LOAD_DIR = "trained_pixel_art_model"
COLOR_MAP_FILE = os.path.join(MODEL_LOAD_DIR, "color_map.json")
# Updated DECODER_MODEL_PATH to include the .keras extension
DECODER_MODEL_PATH = os.path.join(MODEL_LOAD_DIR, "decoder_model.keras")

# Latent dimension must match the one used during training
LATENT_DIM = 256  # Ensure this matches the LATENT_DIM from your training script


# --- Define the Decoder's architecture (must match the saved model) ---
def build_decoder(latent_dim, output_shape=(25, 25, 1), num_colors=None):
    """Builds the decoder part of the VAE."""
    if num_colors is None:
        raise ValueError(
            "num_colors must be provided to build the decoder for generation."
        )

    decoder_inputs = keras.Input(shape=(latent_dim,))
    # Adjust initial dense layer size based on encoder's last conv feature map
    initial_dense_dim = 4 * 4 * 128  # Adjusted based on deeper encoder
    x = layers.Dense(initial_dense_dim, activation="relu")(decoder_inputs)
    x = layers.Reshape((4, 4, 128))(x)

    x = layers.Conv2DTranspose(128, 3, activation="relu", strides=2, padding="same")(
        x
    )  # Added layer
    x = layers.Conv2DTranspose(64, 3, activation="relu", strides=2, padding="same")(x)
    x = layers.Conv2DTranspose(32, 3, activation="relu", strides=2, padding="same")(x)

    # Output layer: now outputs NUM_COLORS channels with softmax for probability distribution
    decoder_outputs = layers.Conv2DTranspose(
        num_colors, 3, activation="softmax", padding="same"
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
    generated_frames_probs = vae_decoder.predict(random_latent_vectors)

    # Get the index with the highest probability for each pixel
    generated_frames_indices = (
        tf.argmax(generated_frames_probs, axis=-1).numpy().astype(int)
    )

    print(
        f"\nGenerated Indices Min: {np.min(generated_frames_indices):.2f}, Max: {np.max(generated_frames_indices):.2f}"
    )

    print(f"\nGenerated {num_samples} new pixel art frames:")
    plt.figure(figsize=(num_samples * 3, 4))

    for i, skin_frame_indices in enumerate(generated_frames_indices):
        # Ensure indices are within the valid range of the color map
        clamped_indices = np.clip(skin_frame_indices, 0, len(color_map) - 1)

        skin_frame_2d_indices = clamped_indices.squeeze()
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
            # Load the color map first to get NUM_COLORS
            with open(COLOR_MAP_FILE, "r") as f:
                loaded_color_map_list = json.load(f)
            loaded_color_map_rgba = np.array(loaded_color_map_list, dtype=np.float32)
            NUM_COLORS_LOADED = len(loaded_color_map_rgba)

            # Load the decoder model using keras.models.load_model for .keras format
            loaded_decoder = build_decoder(LATENT_DIM, num_colors=NUM_COLORS_LOADED)
            loaded_decoder.load_weights(DECODER_MODEL_PATH)

            print(f"Model loaded successfully from {DECODER_MODEL_PATH}")
            print(f"Color map loaded successfully from {COLOR_MAP_FILE}")
            print(f"Loaded Color Map (Normalized RGBA):")
            print(loaded_color_map_rgba)
            print(f"Number of colors in loaded map: {NUM_COLORS_LOADED}")

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
