import os

# Remove CPU-only enforcement to enable GPU usage.
# os.environ["CUDA_VISIBLE_DEVICES"] = "-1" # Removed this line

# Set XLA_FLAGS to point to the CUDA data directory where libdevice is located.
# Based on your provided path: /root/.conda/envs/cuda_env/nvvm/libdevice
# The --xla_gpu_cuda_data_dir should point to the parent directory of 'nvvm'.
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
# This will print the value of XLA_FLAGS as seen by the Python script
# It helps confirm if the environment variable is correctly set before TensorFlow initializes.
xla_flags_env = os.environ.get("XLA_FLAGS")
print(f"XLA_FLAGS environment variable (inside script): {xla_flags_env}")
# Also check TF_XLA_FLAGS if it's set
tf_xla_flags_env = os.environ.get("TF_XLA_FLAGS")
print(f"TF_XLA_FLAGS environment variable (inside script): {tf_xla_flags_env}")

# Confirming GPU usage attempt
print("TensorFlow is configured to attempt running on GPU.")


# --- 1. Configuration and Data Loading ---

# Define the target dimensions for your pixel art frames
# All input frames will be resized (cropped or padded) to these dimensions.
PIXEL_WIDTH = 25
PIXEL_HEIGHT = 25

# Latent dimension for the VAE
LATENT_DIM = 128  # Increased latent dimension for more capacity

# Directory containing your JSON skin files
# This path is based on your provided information.
SKIN_DATA_DIR = "/home/dd/cyberia-client/object_layer/skin"

# Directory to save the trained model and color map
MODEL_SAVE_DIR = "trained_pixel_art_model"
COLOR_MAP_FILE = os.path.join(MODEL_SAVE_DIR, "color_map.json")
# Updated DECODER_MODEL_PATH to include the .keras extension
DECODER_MODEL_PATH = os.path.join(MODEL_SAVE_DIR, "decoder_model.keras")


def preprocess_frame(frame_array, target_height, target_width):
    """
    Resizes (crops or pads) a single pixel art frame to the target dimensions.
    Pads with zeros if smaller, center-crops if larger.
    """
    current_height, current_width = frame_array.shape

    # Create a new array filled with zeros for padding
    processed_frame = np.zeros((target_height, target_width), dtype=frame_array.dtype)

    # Calculate cropping/padding offsets
    h_start = max(0, (current_height - target_height) // 2)
    w_start = max(0, (current_width - target_width) // 2)

    h_end = h_start + min(current_height, target_height)
    w_end = w_start + min(current_width, target_width)

    target_h_start = max(0, (target_height - current_height) // 2)
    target_w_start = max(0, (target_width - current_width) // 2)

    target_h_end = target_h_start + min(current_height, target_height)
    target_w_end = target_w_start + min(current_width, target_width)

    # Place the original frame (or its cropped part) into the new array
    processed_frame[target_h_start:target_h_end, target_w_start:target_w_end] = (
        frame_array[h_start:h_end, w_start:w_end]
    )

    return processed_frame


def load_and_preprocess_data_from_file(json_file_path, target_height, target_width):
    """
    Loads pixel art data from a single JSON file and preprocesses it.
    Dynamically determines NUM_COLORS, resizes frames, and extracts the color map.
    """
    # Check if the file exists before attempting to open it
    if not os.path.exists(json_file_path):
        print(f"Error: File not found at {json_file_path}")
        return np.array([]), 0, np.array([])

    with open(json_file_path, "r") as f:
        data = json.load(f)

    all_frames_in_file = []
    num_colors_in_file = 0
    color_map_rgba_in_file = np.array([])  # Initialize empty color map

    # Determine NUM_COLORS dynamically from the 'COLORS' array and extract the color map
    if "COLORS" in data["RENDER_DATA"] and data["RENDER_DATA"]["COLORS"]:
        num_colors_in_file = len(data["RENDER_DATA"]["COLORS"])
        # Convert color map to a NumPy array and normalize RGBA values to 0-1 range
        color_map_rgba_in_file = (
            np.array(data["RENDER_DATA"]["COLORS"], dtype=np.float32) / 255.0
        )
    else:
        print(
            f"Warning: 'COLORS' array not found or empty in {json_file_path}. Assuming NUM_COLORS = 4 and a default grayscale color map."
        )
        num_colors_in_file = 4  # Default to 4 if not found
        # Create a default grayscale color map if not provided
        color_map_rgba_in_file = np.array(
            [
                [0.0, 0.0, 0.0, 1.0],  # Black
                [0.33, 0.33, 0.33, 1.0],  # Dark Gray
                [0.66, 0.66, 0.66, 1.0],  # Light Gray
                [1.0, 1.0, 1.0, 1.0],  # White
            ],
            dtype=np.float32,
        )

    # Iterate through all animation states (e.g., UP_IDLE, DOWN_WALKING)
    for animation_state in data["RENDER_DATA"]["FRAMES"]:
        for frame_list in data["RENDER_DATA"]["FRAMES"][animation_state]:
            frame_array = np.array(frame_list, dtype=np.float32)
            processed_frame = preprocess_frame(frame_array, target_height, target_width)
            all_frames_in_file.append(processed_frame)

    if not all_frames_in_file:
        print(f"No valid frames found in {json_file_path}")
        return np.array([]), 0, np.array([])

    # Convert list of frames to a numpy array
    frames_array = np.array(all_frames_in_file)

    # Normalize pixel values (indices) to be between 0 and 1
    # Use the dynamically determined num_colors_in_file for normalization
    # IMPORTANT: Ensure that the input pixel values are indeed indices from 0 to num_colors_in_file - 1.
    # If they are already scaled values, this normalization might be incorrect.
    normalized_frames = (
        frames_array / (num_colors_in_file - 1)
        if num_colors_in_file > 1
        else frames_array
    )

    # Reshape for CNN input: (num_samples, height, width, channels)
    input_shape = (normalized_frames.shape[0], target_height, target_width, 1)
    return (
        normalized_frames.reshape(input_shape),
        num_colors_in_file,
        color_map_rgba_in_file,
    )


# --- Load data from all JSON files in the specified directory ---
all_skin_files = [
    f
    for f in os.listdir(SKIN_DATA_DIR)
    if f.startswith("object_layer_data_") and f.endswith(".json")
]
all_skin_files_paths = [os.path.join(SKIN_DATA_DIR, f) for f in all_skin_files]

all_preprocessed_frames = []
max_num_colors = 0
final_color_map_rgba = np.array([])

if not all_skin_files_paths:
    print(
        f"No JSON files found in {SKIN_DATA_DIR}. Please ensure files are named 'object_layer_data_*.json'."
    )
    # Fallback to dummy data if no files are found
    x_train = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1).astype(np.float32)
    NUM_COLORS = 4  # Default to 4 for dummy data
    COLOR_MAP_RGBA = np.array(
        [
            [0.0, 0.0, 0.0, 1.0],  # Black
            [0.33, 0.33, 0.33, 1.0],  # Dark Gray
            [0.66, 0.66, 0.66, 1.0],  # Light Gray
            [1.0, 1.0, 1.0, 1.0],  # White
        ],
        dtype=np.float32,
    )
    print(f"Using dummy data of shape: {x_train.shape} and NUM_COLORS={NUM_COLORS}")
else:
    try:
        for skin_file_path in all_skin_files_paths:
            print(f"Loading data from: {skin_file_path}")
            frames, current_num_colors, current_color_map_rgba = (
                load_and_preprocess_data_from_file(
                    skin_file_path, PIXEL_HEIGHT, PIXEL_WIDTH
                )
            )
            if frames.size > 0:
                all_preprocessed_frames.append(frames)
                # Keep the color map from the file with the largest number of colors
                # This assumes a more comprehensive palette is desired.
                if current_num_colors > max_num_colors:
                    max_num_colors = current_num_colors
                    final_color_map_rgba = current_color_map_rgba

        if all_preprocessed_frames:
            x_train = np.concatenate(all_preprocessed_frames, axis=0)
            NUM_COLORS = max_num_colors
            COLOR_MAP_RGBA = final_color_map_rgba
            print(f"Total number of frames loaded: {x_train.shape[0]}")
            print(f"Shape of loaded training data: {x_train.shape}")
            print(f"Dynamically determined number of colors: {NUM_COLORS}")
            print(f"Shape of final color map: {COLOR_MAP_RGBA.shape}")
            print(f"\n--- Final Color Map (Normalized RGBA) ---")
            print(COLOR_MAP_RGBA)  # Print the color map for inspection
            print(f"----------------------------------------")
        else:
            raise ValueError("No valid frames loaded from any JSON file.")

    except Exception as e:
        print(f"Error loading data from multiple files: {e}")
        print("Falling back to dummy data for VAE training.")
        # Fallback to dummy data if loading fails for any reason
        x_train = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1).astype(np.float32)
        NUM_COLORS = 4  # Default to 4 for dummy data
        COLOR_MAP_RGBA = np.array(
            [
                [0.0, 0.0, 0.0, 1.0],  # Black
                [0.33, 0.33, 0.33, 1.0],  # Dark Gray
                [0.66, 0.66, 0.66, 1.0],  # Light Gray
                [1.0, 1.0, 1.0, 1.0],  # White
            ],
            dtype=np.float32,
        )
        print(f"Using dummy data of shape: {x_train.shape} and NUM_COLORS={NUM_COLORS}")

# --- Add Synthetic Data (Random Pixel Art) ---
NUM_SYNTHETIC_FRAMES = 500  # Number of synthetic frames to generate
if x_train.size > 0 and NUM_COLORS > 0:
    print(f"\nGenerating {NUM_SYNTHETIC_FRAMES} synthetic frames...")
    # Generate random integers from 0 to NUM_COLORS-1 for synthetic data
    synthetic_frames = np.random.randint(
        0, NUM_COLORS, size=(NUM_SYNTHETIC_FRAMES, PIXEL_HEIGHT, PIXEL_WIDTH, 1)
    ).astype(np.float32)
    # Normalize synthetic frames using the determined NUM_COLORS
    synthetic_frames_normalized = (
        synthetic_frames / (NUM_COLORS - 1) if NUM_COLORS > 1 else synthetic_frames
    )
    x_train = np.concatenate([x_train, synthetic_frames_normalized], axis=0)
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
    x = layers.Flatten()(x)
    x = layers.Dense(128, activation="relu")(
        x
    )  # Added a dense layer before latent space
    z_mean = layers.Dense(latent_dim, name="z_mean")(x)
    z_log_var = layers.Dense(latent_dim, name="z_log_var")(x)
    z = Sampling()([z_mean, z_log_var])
    return keras.Model(encoder_inputs, [z_mean, z_log_var, z], name="encoder")


def build_decoder(latent_dim, output_shape):
    """Builds the decoder part of the VAE."""
    decoder_inputs = keras.Input(shape=(latent_dim,))
    # Calculate initial dense layer size based on encoder's last conv feature map
    # Encoder output before flatten: (None, 7, 7, 64) for 25x25 input
    initial_dense_dim = 7 * 7 * 64
    x = layers.Dense(initial_dense_dim, activation="relu")(decoder_inputs)
    x = layers.Reshape((7, 7, 64))(x)  # Reshape to a 2D feature map

    # Upsample to 14x14 (7 * 2 = 14)
    x = layers.Conv2DTranspose(64, 3, activation="relu", strides=2, padding="same")(x)
    # Upsample to 28x28 (14 * 2 = 28)
    x = layers.Conv2DTranspose(32, 3, activation="relu", strides=2, padding="same")(x)
    # Output layer: single channel (for pixel values 0-3), sigmoid for 0-1 range
    # The sigmoid activation maps output to [0, 1].
    # We then denormalize and round to get integer indices.
    decoder_outputs = layers.Conv2DTranspose(
        1, 3, activation="sigmoid", padding="same"
    )(x)

    # Crop the 28x28 output to the target output_shape (e.g., 25x25)
    # The cropping values depend on the difference between the current output (28x28)
    # and the target output_shape (25x25).
    # (28 - 25) = 3 pixels difference in height and width.
    # We need to remove 3 pixels from height and 3 pixels from width.
    # Cropping expects ((top_crop, bottom_crop), (left_crop, right_crop))
    # To remove 3 pixels, we can remove 1 from top/left and 2 from bottom/right, or vice-versa.
    # This ensures the output matches the target PIXEL_HEIGHT and PIXEL_WIDTH.
    crop_height = decoder_outputs.shape[1] - output_shape[0]
    crop_width = decoder_outputs.shape[2] - output_shape[1]

    # Ensure cropping values are non-negative
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
    def __init__(self, encoder, decoder, **kwargs):
        super().__init__(**kwargs)
        self.encoder = encoder
        self.decoder = decoder
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
        with tf.GradientTape() as tape:
            z_mean, z_log_var, z = self.encoder(data)
            reconstruction = self.decoder(z)
            reconstruction_loss = tf.reduce_mean(
                keras.losses.binary_crossentropy(data, reconstruction)
            )
            kl_loss = -0.5 * (1 + z_log_var - tf.square(z_mean) - tf.exp(z_log_var))
            kl_loss = tf.reduce_mean(tf.reduce_sum(kl_loss, axis=1))
            total_loss = reconstruction_loss + kl_loss
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
decoder = build_decoder(LATENT_DIM, (PIXEL_HEIGHT, PIXEL_WIDTH, 1))

# Create and compile the VAE model
# Consider a slightly lower learning rate if the model is unstable or not converging well
vae = VAE(encoder, decoder)
vae.compile(
    optimizer=keras.optimizers.Adam(learning_rate=0.0005)
)  # Slightly reduced learning rate

# --- 3. Train the VAE ---
EPOCHS = 50  # Down to 50 epochs as requested
BATCH_SIZE = 32

if x_train.size > 0:
    print("\nStarting VAE training...")
    vae.fit(x_train, epochs=EPOCHS, batch_size=BATCH_SIZE)
    print("VAE training complete.")

    # --- Save the trained decoder model and color map ---
    print(f"\nSaving trained decoder model to: {DECODER_MODEL_PATH}")
    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)  # Create directory if it doesn't exist
    # Corrected: Added .keras extension to the save path
    vae.decoder.save(DECODER_MODEL_PATH)
    print(f"Decoder model saved successfully.")

    print(f"Saving color map to: {COLOR_MAP_FILE}")
    # Convert numpy array to list for JSON serialization
    with open(COLOR_MAP_FILE, "w") as f:
        json.dump(COLOR_MAP_RGBA.tolist(), f)
    print(f"Color map saved successfully.")

else:
    print("Cannot train VAE: No valid training data available.")

# --- 4. Generate New Skins and Visualize (from currently trained model) ---


def generate_and_visualize_skin(vae_decoder, latent_dim, color_map, num_samples=1):
    """Generates new pixel art frames and visualizes them using the provided color map."""
    # Sample random points from the latent space (standard normal distribution)
    random_latent_vectors = tf.random.normal(shape=(num_samples, latent_dim))
    # Decode these vectors into pixel art frames (normalized indices)
    generated_frames_normalized = vae_decoder.predict(random_latent_vectors)

    # --- Debugging: Print min/max of generated normalized values ---
    print(
        f"\nGenerated Normalized Frames (Raw Sigmoid Output) Min: {np.min(generated_frames_normalized):.4f}, Max: {np.max(generated_frames_normalized):.4f}"
    )

    # Denormalize pixel values back to original integer indices (0 to NUM_COLORS-1)
    # Use the max index from the color_map for denormalization (len(color_map) - 1)
    max_color_index = len(color_map) - 1
    generated_frames_denormalized_before_clip = (
        generated_frames_normalized * max_color_index
    )
    generated_frames_indices = np.round(generated_frames_denormalized_before_clip)
    generated_frames_indices = np.clip(
        generated_frames_indices, 0, max_color_index
    ).astype(int)

    # --- Debugging: Print min/max of generated indices (before and after clip) ---
    print(
        f"Generated Indices (Before Round & Clip) Min: {np.min(generated_frames_denormalized_before_clip):.2f}, Max: {np.max(generated_frames_denormalized_before_clip):.2f}"
    )
    print(
        f"Generated Indices (After Round & Clip) Min: {np.min(generated_frames_indices):.2f}, Max: {np.max(generated_frames_indices):.2f}"
    )

    print(f"\nGenerated {num_samples} new pixel art frames:")
    plt.figure(
        figsize=(num_samples * 3, 4)
    )  # Adjust figure size based on number of samples

    for i, skin_frame_indices in enumerate(generated_frames_indices):
        # Convert index frame to RGBA frame using the color map
        # Reshape to 2D array of indices (remove the channel dimension)
        skin_frame_2d_indices = skin_frame_indices.squeeze()
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


if x_train.size > 0 and COLOR_MAP_RGBA.size > 0:
    print(
        "\nGenerating and visualizing new pixel art frames (from currently trained model)..."
    )
    num_generations = 5
    generate_and_visualize_skin(
        vae.decoder, LATENT_DIM, COLOR_MAP_RGBA, num_samples=num_generations
    )
else:
    print(
        "\nSkipping generation and visualization: VAE was not trained or no valid color map available."
    )
