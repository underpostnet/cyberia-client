import os
import random  # Import random for shuffling
from scipy.ndimage import shift, rotate  # For more sophisticated augmentations
from collections import Counter  # For finding most common colors

# Set XLA_FLAGS to point to the CUDA data directory where libdevice is located.
# This must be set BEFORE importing tensorflow.
os.environ["XLA_FLAGS"] = "--xla_gpu_cuda_data_dir=/root/.conda/envs/cuda_env"

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np
import json
import matplotlib.pyplot as plt  # Import matplotlib for visualization

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
    except RuntimeError as e:
        # Visible devices must be set before GPUs have been initialized
        print(e)
else:
    print("TensorFlow did not detect any GPU devices. Running on CPU.")


# --- Environment Variable Check (for debugging) ---
xla_flags_env = os.environ.get("XLA_FLAGS")
print(f"XLA_FLAGS environment variable (inside script): {xla_flags_env}")
tf_xla_flags_env = os.environ.get("TF_XLA_FLAGS")
print(f"TF_XLA_FLAGS environment variable (inside script): {tf_xla_flags_env}")

print("TensorFlow is configured to attempt running on GPU.")


# --- 1. Configuration and Data Loading ---

# Define the target dimensions for your pixel art frames
PIXEL_WIDTH = 25
PIXEL_HEIGHT = 25

# Latent dimension for the VAE - Increased for more capacity
LATENT_DIM = 256

# Directory containing your JSON skin files
SKIN_DATA_DIR = "/home/dd/cyberia-client/object_layer/skin"

# Directory to save the trained model and color map
MODEL_SAVE_DIR = "."
COLOR_MAP_FILE = os.path.join(MODEL_SAVE_DIR, "color_map.json")
DECODER_MODEL_PATH = os.path.join(MODEL_SAVE_DIR, "decoder_model.keras")

# Global variables for the dynamically generated color map and number of colors
GLOBAL_COLOR_MAP_RGBA = np.array([])
GLOBAL_NUM_COLORS = 0

# KL Divergence weight - Adjusted to encourage more diverse generations
# Adjusted KL_WEIGHT for better balance
KL_WEIGHT = 0.0001

# Maximum number of colors in the global palette
MAX_GLOBAL_COLORS = 256  # Standard pixel art palette size


def preprocess_frame(frame_array, target_height, target_width):
    """
    Resizes (crops or pads) a single pixel art frame to the target dimensions.
    Pads with zeros if smaller, center-crops if larger.
    Returns a 2D array (height, width).
    """
    current_height, current_width = frame_array.shape
    processed_frame = np.zeros((target_height, target_width), dtype=frame_array.dtype)

    h_start = max(0, (current_height - target_height) // 2)
    w_start = max(0, (current_width - target_width) // 2)
    h_end = h_start + min(current_height, target_height)
    w_end = w_start + min(current_width, target_width)

    target_h_start = max(0, (target_height - current_height) // 2)
    target_w_start = max(0, (target_width - current_width) // 2)
    target_h_end = target_h_start + min(current_height, target_height)
    target_w_end = target_w_start + min(current_width, target_width)

    processed_frame[target_h_start:target_h_end, target_w_start:target_w_end] = (
        frame_array[h_start:h_end, w_start:w_end]
    )

    return processed_frame


def find_closest_color_index(target_color_tuple, palette_rgba):
    """
    Finds the index of the closest color in the given palette (RGBA)
    to the target_color_tuple (RGBA). Uses Euclidean distance.
    """
    target_color_np = np.array(target_color_tuple, dtype=np.float32) / 255.0
    distances = np.sum((palette_rgba - target_color_np) ** 2, axis=1)
    return np.argmin(distances)


def build_global_color_map(all_skin_files_paths, max_colors=MAX_GLOBAL_COLORS):
    """
    Builds a comprehensive, unique color map from all JSON files,
    then truncates it to max_colors by selecting the most frequent ones.
    Returns the unique color map and a dictionary for quick index lookup.
    """
    all_colors_list = []
    for json_file_path in all_skin_files_paths:
        if not os.path.exists(json_file_path):
            print(
                f"Warning: File not found at {json_file_path}, skipping color extraction."
            )
            continue
        with open(json_file_path, "r") as f:
            data = json.load(f)
        if "COLORS" in data["RENDER_DATA"] and data["RENDER_DATA"]["COLORS"]:
            for color_rgba in data["RENDER_DATA"]["COLORS"]:
                all_colors_list.append(tuple(color_rgba))  # Use tuple for hashing

    print(
        f"Initial unique colors found across all selected files: {len(set(all_colors_list))}"
    )

    # Count color frequencies
    color_counts = Counter(all_colors_list)

    # Get the most common colors up to max_colors
    most_common_colors = [color for color, _ in color_counts.most_common(max_colors)]

    # Ensure (0,0,0,0) (transparent/black) is always included as index 0 for consistency
    if (0, 0, 0, 0) not in most_common_colors:
        # If not among most common, add it at the beginning if space allows, or replace least common
        if len(most_common_colors) < max_colors:
            most_common_colors.insert(0, (0, 0, 0, 0))
        else:  # Replace the least common if palette is full
            # Find the least common color that is NOT (0,0,0,0) if it exists, to replace it
            least_common_to_replace = None
            for color, count in reversed(
                color_counts.most_common()
            ):  # Iterate from least common
                if color != (0, 0, 0, 0) and color in most_common_colors:
                    least_common_to_replace = color
                    break
            if least_common_to_replace:
                most_common_colors.remove(least_common_to_replace)
            most_common_colors.insert(0, (0, 0, 0, 0))
            # Ensure we don't exceed max_colors if we added one
            if len(most_common_colors) > max_colors:
                most_common_colors = most_common_colors[:max_colors]

    # Sort the final selected unique colors for consistent indexing
    # We sort to ensure determinism, but the order might not be visually intuitive.
    sorted_selected_colors = sorted(
        list(set(most_common_colors))
    )  # Use set to remove duplicates if (0,0,0,0) was already there

    # Create a mapping from original RGBA tuple to new global index
    color_to_global_index = {
        color_tuple: i for i, color_tuple in enumerate(sorted_selected_colors)
    }

    # Convert to NumPy array and normalize to 0-1 range
    global_color_map = np.array(sorted_selected_colors, dtype=np.float32) / 255.0

    print(
        f"Final global color map size (capped at {max_colors}): {len(global_color_map)}"
    )
    return global_color_map, color_to_global_index


def load_and_preprocess_data_from_file(
    json_file_path,
    target_height,
    target_width,
    global_color_map_rgba,
    color_to_global_index,
    max_frames_per_file=None,
):
    """
    Loads pixel art data from a single JSON file, preprocesses it,
    and maps local color indices to global color indices (or closest in global map).
    Returns 2D arrays (height, width) for each frame.
    Optionally limits the number of frames loaded per file.
    """
    if not os.path.exists(json_file_path):
        print(
            f"Warning: File not found at {json_file_path}, skipping color extraction."
        )
        return [], []

    with open(json_file_path, "r") as f:
        data = json.load(f)

    frames_2d_in_file_normalized = []  # Store normalized 2D frames for X input
    frames_2d_in_file_integer = []  # Store integer 2D frames for Y input

    # Get the local color map for this specific file
    local_color_map = data["RENDER_DATA"].get("COLORS", [])
    if not local_color_map:
        print(
            f"Warning: 'COLORS' array not found or empty in {json_file_path}. Skipping frames from this file."
        )
        return [], []

    # Iterate only through the "RIGHT_IDLE" animation state
    if "RIGHT_IDLE" in data["RENDER_DATA"]["FRAMES"]:
        frames_loaded_count = 0
        for frame_list in data["RENDER_DATA"]["FRAMES"]["RIGHT_IDLE"]:
            if (
                max_frames_per_file is not None
                and frames_loaded_count >= max_frames_per_file
            ):
                break  # Stop loading if max_frames_per_file is reached

            frame_array = np.array(
                frame_list, dtype=np.int32
            )  # Ensure integer type for indices

            # Map local indices to original RGBA colors
            original_rgba_frame = np.array(
                [local_color_map[idx] for idx in frame_array.flatten()]
            ).reshape(frame_array.shape[0], frame_array.shape[1], 4)

            # Convert RGBA frame to global indexed frame using the limited palette
            global_indexed_frame_2d = np.zeros(frame_array.shape, dtype=np.int32)
            for r_idx in range(original_rgba_frame.shape[0]):
                for c_idx in range(original_rgba_frame.shape[1]):
                    rgba_tuple = tuple(original_rgba_frame[r_idx, c_idx])
                    if rgba_tuple in color_to_global_index:
                        global_indexed_frame_2d[r_idx, c_idx] = color_to_global_index[
                            rgba_tuple
                        ]
                    else:
                        # Find closest color in the global palette
                        closest_idx = find_closest_color_index(
                            rgba_tuple, global_color_map_rgba
                        )
                        global_indexed_frame_2d[r_idx, c_idx] = closest_idx

            processed_frame_2d = preprocess_frame(
                global_indexed_frame_2d, target_height, target_width
            )

            frames_2d_in_file_integer.append(processed_frame_2d)
            # Normalize for X input (still 2D at this point)
            normalized_frame_2d = (
                processed_frame_2d.astype(np.float32) / (GLOBAL_NUM_COLORS - 1)
                if GLOBAL_NUM_COLORS > 1
                else processed_frame_2d.astype(np.float32)
            )
            frames_2d_in_file_normalized.append(normalized_frame_2d)

            frames_loaded_count += 1
    else:
        print(
            f"Warning: 'RIGHT_IDLE' frames not found in {json_file_path}. Skipping this file for training."
        )

    return frames_2d_in_file_normalized, frames_2d_in_file_integer


def augment_frame_2d(frame_2d_normalized, frame_2d_integer, num_colors):
    """
    Applies 5 types of data augmentation to a single 2D frame.
    Returns a list of (normalized_frame, integer_frame) tuples.
    """
    augmented_frames_x = []
    augmented_frames_y = []

    # 1. Original
    augmented_frames_x.append(frame_2d_normalized)
    augmented_frames_y.append(frame_2d_integer)

    # 2. Horizontal Flip
    flipped_frame_x = np.flip(frame_2d_normalized, axis=1)
    flipped_frame_y = np.flip(frame_2d_integer, axis=1)
    augmented_frames_x.append(flipped_frame_x)
    augmented_frames_y.append(flipped_frame_y)

    # 3. Random Shift (Translation)
    # Max shift of +/- 2 pixels in any direction
    shift_x = random.randint(-2, 2)
    shift_y = random.randint(-2, 2)
    # Use order=0 for nearest neighbor interpolation, crucial for pixel art
    shifted_frame_y = shift(
        frame_2d_integer, (shift_y, shift_x), mode="nearest", order=0
    )
    shifted_frame_x = (
        shifted_frame_y.astype(np.float32) / (num_colors - 1)
        if num_colors > 1
        else shifted_frame_y.astype(np.float32)
    )
    augmented_frames_x.append(shifted_frame_x)
    augmented_frames_y.append(shifted_frame_y)

    # 4. Random 90-degree Rotation
    # Rotate by 0, 90, 180, or 270 degrees
    k = random.randint(0, 3)  # Number of 90-degree rotations
    rotated_frame_y = np.rot90(frame_2d_integer, k=k)
    rotated_frame_x = (
        rotated_frame_y.astype(np.float32) / (num_colors - 1)
        if num_colors > 1
        else rotated_frame_y.astype(np.float32)
    )
    augmented_frames_x.append(rotated_frame_x)
    augmented_frames_y.append(rotated_frame_y)

    # 5. Random Pixel Noise
    noisy_frame_y = np.copy(frame_2d_integer)
    num_pixels = noisy_frame_y.shape[0] * noisy_frame_y.shape[1]
    num_noisy_pixels = int(num_pixels * 0.01)  # 1% noise

    if num_noisy_pixels == 0 and num_pixels > 0:
        num_noisy_pixels = 1

    if num_pixels > 0:
        rows = np.random.randint(0, noisy_frame_y.shape[0], size=num_noisy_pixels)
        cols = np.random.randint(0, noisy_frame_y.shape[1], size=num_noisy_pixels)
        random_indices = np.random.randint(0, num_colors, size=num_noisy_pixels)
        noisy_frame_y[rows, cols] = random_indices

    noisy_frame_x = (
        noisy_frame_y.astype(np.float32) / (num_colors - 1)
        if num_colors > 1
        else noisy_frame_y.astype(np.float32)
    )
    augmented_frames_x.append(noisy_frame_x)
    augmented_frames_y.append(noisy_frame_y)

    return augmented_frames_x, augmented_frames_y


# --- Load data from all JSON files in the specified directory ---
all_available_skin_files = [
    f
    for f in os.listdir(SKIN_DATA_DIR)
    if f.startswith("object_layer_data_")
    and f.endswith(".json")
    and not f
    in [
        "object_layer_data_dog.json",
        "object_layer_data_ghost.json",
        "object_layer_data_green.json",
        "object_layer_data_kishins.json",
        "object_layer_data_purple.json",
        "object_layer_data_anon.json",
        "object_layer_data_scp-2040.json",
        "object_layer_data_marciano.json",
        "object_layer_data_odisea.json",
    ]
]
random.shuffle(all_available_skin_files)  # Shuffle to get a random subset

# Select up to 26 files
NUM_SELECTED_FILES = 26
selected_skin_files = all_available_skin_files[
    : min(NUM_SELECTED_FILES, len(all_available_skin_files))
]
selected_skin_files_paths = [
    os.path.join(SKIN_DATA_DIR, f) for f in selected_skin_files
]  # Corrected SK_DATA_DIR to SKIN_DATA_DIR

print(f"Selected {len(selected_skin_files)} files for training.")

# Build the global color map first
if not selected_skin_files_paths:
    print(
        f"No JSON files found in {SKIN_DATA_DIR}. Please ensure files are named 'object_layer_data_*.json'."
    )
    # Fallback to dummy data if no files are found
    GLOBAL_COLOR_MAP_RGBA = np.array(
        [[0.0, 0.0, 0.0, 0.0], [1.0, 1.0, 1.0, 1.0]], dtype=np.float32
    )
    GLOBAL_NUM_COLORS = len(GLOBAL_COLOR_MAP_RGBA)
    x_train_2d = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH).astype(np.float32)
    y_train_indices_2d = np.random.randint(
        0, GLOBAL_NUM_COLORS, size=(100, PIXEL_HEIGHT, PIXEL_WIDTH)
    ).astype(np.int32)
    print(
        f"Using dummy data of shape: {x_train_2d.shape} and NUM_COLORS={GLOBAL_NUM_COLORS}"
    )
else:
    GLOBAL_COLOR_MAP_RGBA, color_to_global_index = build_global_color_map(
        selected_skin_files_paths, max_colors=MAX_GLOBAL_COLORS
    )
    GLOBAL_NUM_COLORS = len(GLOBAL_COLOR_MAP_RGBA)
    print(f"Built global color map with {GLOBAL_NUM_COLORS} unique colors.")
    print(f"\n--- Global Color Map (Normalized RGBA) ---")
    print(GLOBAL_COLOR_MAP_RGBA)
    print(f"----------------------------------------")

    all_preprocessed_frames_2d = []  # List to hold 2D normalized frames
    all_integer_frames_2d = []  # List to hold 2D integer frames

    try:
        for skin_file_path in selected_skin_files_paths:
            print(f"Loading data from: {skin_file_path}")
            # Limit to 5 frames per file to increase data density
            frames_normalized_2d_list, frames_integer_2d_list = (
                load_and_preprocess_data_from_file(
                    skin_file_path,
                    PIXEL_HEIGHT,
                    PIXEL_WIDTH,
                    GLOBAL_COLOR_MAP_RGBA,
                    color_to_global_index,
                    max_frames_per_file=5,
                )
            )
            # Apply augmentations to each loaded 2D frame
            for original_x_2d, original_y_2d in zip(
                frames_normalized_2d_list, frames_integer_2d_list
            ):
                augmented_x, augmented_y = augment_frame_2d(
                    original_x_2d, original_y_2d, GLOBAL_NUM_COLORS
                )
                all_preprocessed_frames_2d.extend(augmented_x)
                all_integer_frames_2d.extend(augmented_y)

        if all_preprocessed_frames_2d:
            x_train_real_augmented_2d = np.array(all_preprocessed_frames_2d)
            y_train_indices_real_augmented_2d = np.array(all_integer_frames_2d)

            print(
                f"Total real data after selection, limiting, and 5x augmentation: {x_train_real_augmented_2d.shape[0]} frames."
            )
            print(
                f"Shape of x_train_real_augmented_2d after concatenation: {x_train_real_augmented_2d.shape}"
            )

        else:
            x_train_real_augmented_2d = np.array([])
            y_train_indices_real_augmented_2d = np.array([])
            print("No valid real frames loaded from any JSON file for augmentation.")

    except Exception as e:
        print(f"Error loading and augmenting real data from multiple files: {e}")
        x_train_real_augmented_2d = np.array([])
        y_train_indices_real_augmented_2d = np.array([])
        print("Skipping real data augmentation due to error.")


# --- Combine all data (only real data now, no synthetic) ---
if x_train_real_augmented_2d.size > 0:
    x_train_2d = x_train_real_augmented_2d
    y_train_indices_2d = y_train_indices_real_augmented_2d
else:
    print("No valid training data available. Falling back to dummy data.")
    x_train_2d = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH).astype(np.float32)
    y_train_indices_2d = np.random.randint(
        0, GLOBAL_NUM_COLORS, size=(100, PIXEL_HEIGHT, PIXEL_WIDTH)
    ).astype(np.int32)

print(f"\nFinal combined training data size (2D): {x_train_2d.shape[0]} frames.")
print(f"Shape of final training data (2D): {x_train_2d.shape}")

# --- Final Reshape to 4D for Keras Input ---
# Add the channel dimension (1 for grayscale/indexed images)
x_train = x_train_2d.reshape(-1, PIXEL_HEIGHT, PIXEL_WIDTH, 1)
# y_train_indices should NOT have a channel dimension for sparse_categorical_crossentropy
y_train_indices = y_train_indices_2d.reshape(-1, PIXEL_HEIGHT, PIXEL_WIDTH)

print(f"Shape of final training data (normalized for encoder - 4D): {x_train.shape}")
print(
    f"Shape of final training data (integer indices for loss - 3D): {y_train_indices.shape}"
)


# --- 2. Define the VAE Model ---


class Sampling(layers.Layer):
    """Uses (z_mean, z_log_var) to sample z, the latent vector."""

    def call(self, inputs):
        z_mean, z_log_var = inputs
        batch = tf.shape(z_mean)[0]
        dim = tf.shape(z_mean)[1]
        epsilon = tf.keras.backend.random_normal(shape=(batch, dim))
        return z_mean + tf.exp(0.5 * z_log_var) * epsilon


def build_encoder(latent_dim, input_shape):
    """Builds the encoder part of the VAE."""
    encoder_inputs = keras.Input(shape=input_shape)
    x = layers.Conv2D(32, 3, padding="same")(encoder_inputs)
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Conv2D(64, 3, strides=2, padding="same")(x)
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Conv2D(128, 3, strides=2, padding="same")(x)
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Flatten()(x)
    x = layers.Dense(512)(x)  # No activation here, will be applied after BN
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Dense(256)(x)  # No activation here, will be applied after BN
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    z_mean = layers.Dense(latent_dim, name="z_mean")(x)
    z_log_var = layers.Dense(latent_dim, name="z_log_var")(x)
    z = Sampling()([z_mean, z_log_var])
    return keras.Model(encoder_inputs, [z_mean, z_log_var, z], name="encoder")


def build_decoder(latent_dim, output_shape, num_colors):
    """Builds the decoder part of the VAE."""
    decoder_inputs = keras.Input(shape=(latent_dim,))
    initial_dense_dim = 2 * 2 * 128
    x = layers.Dense(initial_dense_dim)(decoder_inputs)  # No activation here
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Reshape((2, 2, 128))(x)

    # FIX: Added strides=2 to the first Conv2DTranspose to correctly upsample
    x = layers.Conv2DTranspose(128, 3, strides=2, padding="same")(x)  # -> 4x4
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Conv2DTranspose(64, 3, strides=2, padding="same")(x)  # -> 8x8
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    x = layers.Conv2DTranspose(32, 3, strides=2, padding="same")(x)  # -> 16x16
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    # Increased filters in this layer
    x = layers.Conv2DTranspose(32, 3, strides=2, padding="same")(x)  # -> 32x32
    x = layers.BatchNormalization()(x)  # Added BatchNormalization
    x = layers.LeakyReLU(negative_slope=0.2)(x)  # Changed to LeakyReLU, fixed alpha
    # Increased filters in this layer
    x = layers.Conv2DTranspose(16, 3, strides=2, padding="same")(x)  # -> 64x64
    x = layers.BatchNormalization()(x)
    x = layers.LeakyReLU(negative_slope=0.2)(x)

    # Output layer: now outputs NUM_COLORS channels with softmax for probability distribution
    decoder_outputs = layers.Conv2DTranspose(
        num_colors, 3, activation="softmax", padding="same"
    )(
        x
    )  # Still 64x64

    # Crop the 64x64 output to the target output_shape (25x25)
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


class VAE(keras.Model):
    def __init__(self, encoder, decoder, kl_weight, **kwargs):
        super().__init__(**kwargs)
        self.encoder = encoder
        self.decoder = decoder
        self.kl_weight = kl_weight  # Store KL weight
        self.total_loss_tracker = keras.metrics.Mean(name="total_loss")
        self.reconstruction_loss_tracker = keras.metrics.Mean(
            name="reconstruction_loss"
        )
        self.kl_loss_tracker = keras.metrics.Mean(name="kl_loss")

    @property
    def metrics(self):
        return [
            self.total_loss_tracker,
            self.reconstruction_loss_tracker,
            self.kl_loss_tracker,
        ]

    def train_step(self, data):
        # 'data' here is a tuple: (x_normalized, y_true_indices)
        x_normalized, y_true_indices = data

        with tf.GradientTape() as tape:
            z_mean, z_log_var, z = self.encoder(x_normalized)
            reconstruction_logits = self.decoder(
                z
            )  # Output are probabilities for each color class

            # Calculate reconstruction loss using sparse_categorical_crossentropy
            # y_true_indices should be (batch, height, width)
            # reconstruction_logits should be (batch, height, width, num_classes)
            reconstruction_loss = tf.reduce_mean(
                keras.losses.sparse_categorical_crossentropy(
                    y_true_indices, reconstruction_logits
                )
            )

            kl_loss = -0.5 * (1 + z_log_var - tf.square(z_mean) - tf.exp(z_log_var))
            kl_loss = tf.reduce_mean(tf.reduce_sum(kl_loss, axis=1))
            total_loss = reconstruction_loss + (
                self.kl_weight * kl_loss
            )  # Apply KL weight
        grads = tape.gradient(total_loss, self.trainable_weights)
        self.optimizer.apply_gradients(zip(grads, self.trainable_weights))
        self.total_loss_tracker.update_state(total_loss)
        self.reconstruction_loss_tracker.update_state(reconstruction_loss)
        self.kl_loss_tracker.update_state(kl_loss)
        return {
            "loss": self.total_loss_tracker.result(),
            "reconstruction_loss": self.reconstruction_loss_tracker.result(),
            "kl_loss": self.kl_loss_tracker.result(),
        }

    def call(self, inputs):
        z_mean, z_log_var, z = self.encoder(inputs)
        reconstruction = self.decoder(z)
        return reconstruction


# Build the VAE components
encoder = build_encoder(LATENT_DIM, (PIXEL_HEIGHT, PIXEL_WIDTH, 1))
# Print encoder summary to confirm shapes
print("\n--- Encoder Summary ---")
encoder.summary()

# Pass GLOBAL_NUM_COLORS to the decoder
decoder = build_decoder(LATENT_DIM, (PIXEL_HEIGHT, PIXEL_WIDTH, 1), GLOBAL_NUM_COLORS)
# Print decoder summary to confirm shapes
print("\n--- Decoder Summary ---")
decoder.summary()

# Create and compile the VAE model
vae = VAE(encoder, decoder, kl_weight=KL_WEIGHT)  # Pass KL_WEIGHT to VAE
vae.compile(
    optimizer=keras.optimizers.Adam(learning_rate=0.0002)
)  # Slightly reduced learning rate

# --- 3. Train the VAE ---
EPOCHS = 100  # Increased epochs for better learning with multi-class output
BATCH_SIZE = 32

if x_train.size > 0:
    print("\nStarting VAE training...")
    # Pass both normalized input and integer indices as data for training
    vae.fit(x_train, y_train_indices, epochs=EPOCHS, batch_size=BATCH_SIZE)
    print("VAE training complete.")

    # --- Save the trained decoder model and color map ---
    print(f"\nSaving trained decoder model to: {DECODER_MODEL_PATH}")
    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)  # Create directory if it doesn't exist
    vae.decoder.save(DECODER_MODEL_PATH)
    print(f"Decoder model saved successfully.")

    print(f"Saving color map to: {COLOR_MAP_FILE}")
    # Convert numpy array to list for JSON serialization
    with open(COLOR_MAP_FILE, "w") as f:
        json.dump(GLOBAL_COLOR_MAP_RGBA.tolist(), f)  # Save the GLOBAL_COLOR_MAP
    print(f"Color map saved successfully.")

else:
    print("Cannot train VAE: No valid training data available.")

# --- 4. Generate New Skins and Visualize (from currently trained model) ---


def generate_and_visualize_skin(vae_decoder, latent_dim, color_map, num_samples=1):
    """Generates new pixel art frames and visualizes them using the provided color map."""
    # Sample random points from the latent space (standard normal distribution)
    random_latent_vectors = tf.random.normal(shape=(num_samples, latent_dim))
    # Decode these vectors into pixel art frames (probabilities for each color)
    generated_frames_probs = vae_decoder.predict(random_latent_vectors)

    # Get the index with the highest probability for each pixel
    generated_frames_indices = (
        tf.argmax(generated_frames_probs, axis=-1).numpy().astype(int)
    )

    # --- Debugging: Print min/max of generated indices ---
    print(
        f"\nGenerated Indices Min: {np.min(generated_frames_indices):.2f}, Max: {np.max(generated_frames_indices):.2f}"
    )

    print(f"\nGenerated {num_samples} new pixel art frames:")
    plt.figure(
        figsize=(num_samples * 3, 4)
    )  # Adjust figure size based on number of samples

    for i, skin_frame_indices in enumerate(generated_frames_indices):
        # Ensure indices are within the valid range of the color map
        clamped_indices = np.clip(skin_frame_indices, 0, len(color_map) - 1)

        # Convert index frame to RGBA frame using the color map
        # Squeeze to remove the channel dimension if it exists (e.g., (25, 25, 1) -> (25, 25))
        skin_frame_2d_indices = clamped_indices.squeeze()
        # Map each index to its corresponding RGBA color
        skin_frame_rgba = color_map[skin_frame_2d_indices]

        # Plotting the generated frame
        plt.subplot(1, num_samples, i + 1)
        plt.imshow(skin_frame_rgba)
        plt.title(f"Generated {i+1}")
        plt.axis("off")  # Hide axes for cleaner pixel art display

        # Also print the 2D array representation for inspection
        print(f"\n--- Generated Skin Frame {i+1} (Indices) ---")
        for row in skin_frame_2d_indices:
            print(row.tolist())

    plt.tight_layout()  # Adjust subplot params for a tight layout
    plt.show()  # Display the plots


if "x_train" in locals() and x_train.size > 0 and GLOBAL_COLOR_MAP_RGBA.size > 0:
    print(
        "\nGenerating and visualizing new pixel art frames (from currently trained model)..."
    )
    num_generations = 5
    generate_and_visualize_skin(
        vae.decoder, LATENT_DIM, GLOBAL_COLOR_MAP_RGBA, num_samples=num_generations
    )
else:
    print(
        "\nSkipping generation and visualization: VAE was not trained or no valid color map available."
    )
