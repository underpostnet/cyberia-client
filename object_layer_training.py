import os

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
        print(f"Using GPU: {gpus[0].name}")
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
MODEL_SAVE_DIR = "trained_pixel_art_model"
COLOR_MAP_FILE = os.path.join(MODEL_SAVE_DIR, "color_map.json")
DECODER_MODEL_PATH = os.path.join(MODEL_SAVE_DIR, "decoder_model.keras")

# Global variables for the dynamically generated color map and number of colors
GLOBAL_COLOR_MAP_RGBA = np.array([])
GLOBAL_NUM_COLORS = 0

# KL Divergence weight - Adjusted to encourage more diverse generations
KL_WEIGHT = 0.0001  # A smaller weight can lead to more diverse, less blurry outputs


def preprocess_frame(frame_array, target_height, target_width):
    """
    Resizes (crops or pads) a single pixel art frame to the target dimensions.
    Pads with zeros if smaller, center-crops if larger.
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


def build_global_color_map(all_skin_files_paths):
    """
    Builds a comprehensive, unique color map from all JSON files.
    Returns the unique color map and a dictionary for quick index lookup.
    """
    unique_colors_set = set()
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
                unique_colors_set.add(tuple(color_rgba))  # Use tuple for set hashing

    # Convert set back to list and sort for consistent indexing
    sorted_unique_colors = sorted(list(unique_colors_set))

    # Create a mapping from original RGBA tuple to new global index
    color_to_global_index = {
        color_tuple: i for i, color_tuple in enumerate(sorted_unique_colors)
    }

    # Convert to NumPy array and normalize to 0-1 range
    global_color_map = np.array(sorted_unique_colors, dtype=np.float32) / 255.0

    return global_color_map, color_to_global_index


def load_and_preprocess_data_from_file(
    json_file_path, target_height, target_width, color_to_global_index
):
    """
    Loads pixel art data from a single JSON file, preprocesses it,
    and maps local color indices to global color indices.
    Crucially, now only loads frames from "RIGHT_IDLE" and clamps local indices.
    """
    if not os.path.exists(json_file_path):
        print(f"Error: File not found at {json_file_path}")
        return np.array([]), np.array([])

    with open(json_file_path, "r") as f:
        data = json.load(f)

    all_frames_in_file = []

    # Get the local color map for this specific file
    local_color_map = data["RENDER_DATA"].get("COLORS", [])
    if not local_color_map:
        print(
            f"Warning: 'COLORS' array not found or empty in {json_file_path}. Skipping frames from this file."
        )
        return np.array([]), np.array([])

    # Create a mapping from local index to global index
    local_to_global_mapping = np.zeros(len(local_color_map), dtype=np.int32)
    for i, color_rgba in enumerate(local_color_map):
        color_tuple = tuple(color_rgba)
        if color_tuple in color_to_global_index:
            local_to_global_mapping[i] = color_to_global_index[color_tuple]
        else:
            # This should ideally not happen if build_global_color_map is comprehensive
            # If it does, map to index 0 (transparent/black) as a fallback
            print(
                f"Warning: Color {color_tuple} from {json_file_path} not found in global color map. Mapping to index 0."
            )
            local_to_global_mapping[i] = 0

    # Iterate only through the "RIGHT_IDLE" animation state
    if "RIGHT_IDLE" in data["RENDER_DATA"]["FRAMES"]:
        for frame_list in data["RENDER_DATA"]["FRAMES"]["RIGHT_IDLE"]:
            frame_array = np.array(
                frame_list, dtype=np.int32
            )  # Ensure integer type for indices

            # CRITICAL FIX: Clamp local indices to valid range before mapping
            # This prevents errors if original data has indices larger than its own local_color_map
            max_local_index = len(local_color_map) - 1
            frame_array_clamped_local = np.clip(frame_array, 0, max_local_index)

            # Map clamped local indices in the frame to global indices
            global_indexed_frame = local_to_global_mapping[frame_array_clamped_local]

            processed_frame = preprocess_frame(
                global_indexed_frame, target_height, target_width
            )
            all_frames_in_file.append(processed_frame)
    else:
        print(
            f"Warning: 'RIGHT_IDLE' frames not found in {json_file_path}. Skipping this file for training."
        )

    if not all_frames_in_file:
        print(f"No valid 'RIGHT_IDLE' frames found in {json_file_path}")
        return np.array([]), np.array([])

    frames_array = np.array(all_frames_in_file)

    # The frames_array now contains global integer indices.
    # Normalize these for the encoder input.
    normalized_frames = (
        frames_array.astype(np.float32) / (GLOBAL_NUM_COLORS - 1)
        if GLOBAL_NUM_COLORS > 1
        else frames_array.astype(np.float32)
    )

    # Reshape for CNN input: (num_samples, height, width, channels)
    input_shape = (normalized_frames.shape[0], target_height, target_width, 1)

    return normalized_frames.reshape(input_shape), frames_array.reshape(input_shape)


# --- Load data from all JSON files in the specified directory ---
all_skin_files = [
    f
    for f in os.listdir(SKIN_DATA_DIR)
    if f.startswith("object_layer_data_") and f.endswith(".json")
]
all_skin_files_paths = [os.path.join(SKIN_DATA_DIR, f) for f in all_skin_files]

# Build the global color map first
if not all_skin_files_paths:
    print(
        f"No JSON files found in {SKIN_DATA_DIR}. Please ensure files are named 'object_layer_data_*.json'."
    )
    # Fallback to dummy data if no files are found
    GLOBAL_COLOR_MAP_RGBA = np.array(
        [[0.0, 0.0, 0.0, 0.0], [1.0, 1.0, 1.0, 1.0]], dtype=np.float32
    )
    GLOBAL_NUM_COLORS = len(GLOBAL_COLOR_MAP_RGBA)
    x_train = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1).astype(np.float32)
    y_train_indices = np.random.randint(
        0, GLOBAL_NUM_COLORS, size=(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1)
    ).astype(np.int32)
    print(
        f"Using dummy data of shape: {x_train.shape} and NUM_COLORS={GLOBAL_NUM_COLORS}"
    )
else:
    GLOBAL_COLOR_MAP_RGBA, color_to_global_index = build_global_color_map(
        all_skin_files_paths
    )
    GLOBAL_NUM_COLORS = len(GLOBAL_COLOR_MAP_RGBA)
    print(f"Built global color map with {GLOBAL_NUM_COLORS} unique colors.")
    print(f"\n--- Global Color Map (Normalized RGBA) ---")
    print(GLOBAL_COLOR_MAP_RGBA)
    print(f"----------------------------------------")

    all_preprocessed_frames = []
    all_integer_frames = []

    try:
        for skin_file_path in all_skin_files_paths:
            print(f"Loading data from: {skin_file_path}")
            frames_normalized, frames_integer = load_and_preprocess_data_from_file(
                skin_file_path, PIXEL_HEIGHT, PIXEL_WIDTH, color_to_global_index
            )
            if frames_normalized.size > 0:
                all_preprocessed_frames.append(frames_normalized)
                all_integer_frames.append(frames_integer)

        if all_preprocessed_frames:
            x_train_initial = np.concatenate(all_preprocessed_frames, axis=0)
            y_train_indices_initial = np.concatenate(all_integer_frames, axis=0)

            # --- Clone initial data 3 times (total 4x original volume) ---
            x_train = np.concatenate([x_train_initial] * 4, axis=0)
            y_train_indices = np.concatenate([y_train_indices_initial] * 4, axis=0)
            print(
                f"Initial real data cloned 3 times. Total real frames: {x_train.shape[0]}"
            )

            print(
                f"Shape of loaded training data (normalized for encoder): {x_train.shape}"
            )
            print(
                f"Shape of loaded training data (integer indices for loss): {y_train_indices.shape}"
            )
        else:
            raise ValueError("No valid frames loaded from any JSON file.")

    except Exception as e:
        print(f"Error loading data from multiple files: {e}")
        print("Falling back to dummy data for VAE training.")
        x_train = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1).astype(np.float32)
        y_train_indices = np.random.randint(
            0, GLOBAL_NUM_COLORS, size=(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1)
        ).astype(np.int32)
        print(
            f"Using dummy data of shape: {x_train.shape} and NUM_COLORS={GLOBAL_NUM_COLORS}"
        )

# --- Add Synthetic Data (Random Pixel Art) ---
NUM_SYNTHETIC_FRAMES = 500  # Number of synthetic frames to generate
if x_train.size > 0 and GLOBAL_NUM_COLORS > 0:
    print(f"\nGenerating {NUM_SYNTHETIC_FRAMES} synthetic frames...")
    # Generate random integers from 0 to GLOBAL_NUM_COLORS-1 for synthetic data
    synthetic_frames_indices = np.random.randint(
        0, GLOBAL_NUM_COLORS, size=(NUM_SYNTHETIC_FRAMES, PIXEL_HEIGHT, PIXEL_WIDTH, 1)
    ).astype(np.int32)
    # Normalize synthetic frames for input to encoder
    synthetic_frames_normalized = (
        synthetic_frames_indices.astype(np.float32) / (GLOBAL_NUM_COLORS - 1)
        if GLOBAL_NUM_COLORS > 1
        else synthetic_frames_indices.astype(np.float32)
    )

    x_train = np.concatenate([x_train, synthetic_frames_normalized], axis=0)
    y_train_indices = np.concatenate(
        [y_train_indices, synthetic_frames_indices], axis=0
    )

    print(
        f"Total training data after adding synthetic frames: {x_train.shape[0]} frames."
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
    x = layers.Conv2D(32, 3, activation="relu", strides=2, padding="same")(
        encoder_inputs
    )
    x = layers.Conv2D(64, 3, activation="relu", strides=2, padding="same")(x)
    x = layers.Conv2D(128, 3, activation="relu", strides=2, padding="same")(
        x
    )  # Added layer
    x = layers.Flatten()(x)
    x = layers.Dense(256, activation="relu")(x)  # Increased dense layer size
    z_mean = layers.Dense(latent_dim, name="z_mean")(x)
    z_log_var = layers.Dense(latent_dim, name="z_log_var")(x)
    z = Sampling()([z_mean, z_log_var])
    return keras.Model(encoder_inputs, [z_mean, z_log_var, z], name="encoder")


def build_decoder(latent_dim, output_shape, num_colors):
    """Builds the decoder part of the VAE."""
    decoder_inputs = keras.Input(shape=(latent_dim,))
    # Adjust initial dense layer size based on encoder's last conv feature map
    # For a 25x25 input, with 3 conv layers (strides=2 each), the feature map size
    # will be roughly (25 / 2 / 2 / 2) = 3.125, so 3x3 or 4x4. Let's aim for 4x4.
    # If the last encoder conv output is 128 filters, then 4*4*128 = 2048
    initial_dense_dim = 4 * 4 * 128  # Adjusted based on deeper encoder
    x = layers.Dense(initial_dense_dim, activation="relu")(decoder_inputs)
    x = layers.Reshape((4, 4, 128))(x)  # Reshape to a 2D feature map

    # Upsample to 8x8 (4 * 2 = 8)
    x = layers.Conv2DTranspose(128, 3, activation="relu", strides=2, padding="same")(
        x
    )  # Added layer
    # Upsample to 16x16 (8 * 2 = 16)
    x = layers.Conv2DTranspose(64, 3, activation="relu", strides=2, padding="same")(x)
    # Upsample to 32x32 (16 * 2 = 32)
    x = layers.Conv2DTranspose(32, 3, activation="relu", strides=2, padding="same")(x)

    # Output layer: now outputs NUM_COLORS channels with softmax for probability distribution
    decoder_outputs = layers.Conv2DTranspose(
        num_colors, 3, activation="softmax", padding="same"
    )(x)

    # Crop the 32x32 output to the target output_shape (25x25)
    # (32 - 25) = 7 pixels difference in height and width.
    # Cropping expects ((top_crop, bottom_crop), (left_crop, right_crop))
    # To remove 7 pixels, we can remove 3 from top/left and 4 from bottom/right.
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
            )  # Output are probabilities for each class

            # Calculate reconstruction loss using sparse_categorical_crossentropy
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
# Pass GLOBAL_NUM_COLORS to the decoder
decoder = build_decoder(LATENT_DIM, (PIXEL_HEIGHT, PIXEL_WIDTH, 1), GLOBAL_NUM_COLORS)

# Create and compile the VAE model
vae = VAE(encoder, decoder, kl_weight=KL_WEIGHT)  # Pass KL_WEIGHT to VAE
vae.compile(
    optimizer=keras.optimizers.Adam(learning_rate=0.0005)
)  # Slightly reduced learning rate

# --- 3. Train the VAE ---
EPOCHS = 500  # Increased epochs for better learning with multi-class output
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
