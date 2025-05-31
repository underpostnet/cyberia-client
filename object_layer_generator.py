import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np
import json
import os

# --- Force TensorFlow to use CPU (as a last resort for persistent GPU issues) ---
# This line tells TensorFlow to only use CPU devices, bypassing any GPU-related errors.
# If you resolve your GPU setup issues later, you can remove or comment out this line.
tf.config.set_visible_devices([], "GPU")  # Hides all GPU devices from TensorFlow

# --- Environment Variable Check (for debugging) ---
# This will print the value of XLA_FLAGS as seen by the Python script
# It helps confirm if the environment variable is correctly set before TensorFlow initializes.
xla_flags_env = os.environ.get("XLA_FLAGS")
print(f"XLA_FLAGS environment variable (inside script): {xla_flags_env}")
# Also check TF_XLA_FLAGS if it's set
tf_xla_flags_env = os.environ.get("TF_XLA_FLAGS")
print(f"TF_XLA_FLAGS environment variable (inside script): {tf_xla_flags_env}")


# --- 1. Configuration and Data Loading ---

# Define the target dimensions for your pixel art frames
# All input frames will be resized (cropped or padded) to these dimensions.
PIXEL_WIDTH = 25
PIXEL_HEIGHT = 25

# Latent dimension for the VAE
LATENT_DIM = 64

# Directory containing your JSON skin files
# Based on the user's provided path: /home/dd/cyberia-client/object_layer/skin
SKIN_DATA_DIR = "/home/dd/cyberia-client/object_layer/skin"
# For demonstration, we'll use the provided single file.
# In a real scenario, you'd iterate through all files in SKIN_DATA_DIR.
SAMPLE_JSON_FILENAME = "object_layer_data_anon.json"
# Construct the full path to the sample JSON file
SAMPLE_JSON_FILE_PATH = os.path.join(SKIN_DATA_DIR, SAMPLE_JSON_FILENAME)


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


def load_and_preprocess_data(json_file_path, target_height, target_width):
    """
    Loads pixel art data from a single JSON file and preprocesses it.
    Dynamically determines NUM_COLORS and resizes frames.
    """
    # Check if the file exists before attempting to open it
    if not os.path.exists(json_file_path):
        print(f"Error: File not found at {json_file_path}")
        return np.array([]), 0

    with open(json_file_path, "r") as f:
        data = json.load(f)

    all_frames = []
    num_colors_in_data = 0

    # Determine NUM_COLORS dynamically from the 'COLORS' array
    if "COLORS" in data["RENDER_DATA"] and data["RENDER_DATA"]["COLORS"]:
        # The largest index used in the frames is the max pixel value.
        # NUM_COLORS should be max_pixel_value + 1.
        # For example, if colors are [0,1,2,3], max pixel value is 3, so NUM_COLORS is 4.
        # We assume the colors array is ordered by index, and the last index is the highest value.
        num_colors_in_data = len(data["RENDER_DATA"]["COLORS"])
    else:
        print(
            f"Warning: 'COLORS' array not found or empty in {json_file_path}. Assuming NUM_COLORS = 4."
        )
        num_colors_in_data = 4  # Default to 4 if not found

    # Iterate through all animation states (e.g., UP_IDLE, DOWN_WALKING)
    for animation_state in data["RENDER_DATA"]["FRAMES"]:
        for frame_list in data["RENDER_DATA"]["FRAMES"][animation_state]:
            frame_array = np.array(frame_list, dtype=np.float32)
            processed_frame = preprocess_frame(frame_array, target_height, target_width)
            all_frames.append(processed_frame)

    if not all_frames:
        print(f"No valid frames found in {json_file_path}")
        return np.array([]), 0

    # Convert list of frames to a numpy array
    frames_array = np.array(all_frames)

    # Normalize pixel values to be between 0 and 1
    # Use the dynamically determined num_colors_in_data for normalization
    normalized_frames = (
        frames_array / (num_colors_in_data - 1)
        if num_colors_in_data > 1
        else frames_array
    )

    # Reshape for CNN input: (num_samples, height, width, channels)
    input_shape = (normalized_frames.shape[0], target_height, target_width, 1)
    return normalized_frames.reshape(input_shape), num_colors_in_data


# Load data from the sample JSON file
# In a real scenario, you would uncomment and use the loop below to load all files
# from the SKIN_DATA_DIR and concatenate the preprocessed frames.
# Example:
# all_skin_files = [os.path.join(SKIN_DATA_DIR, f) for f in os.listdir(SKIN_DATA_DIR) if f.endswith('.json') and f.startswith('object_layer_data_')]
# all_preprocessed_frames = []
# max_num_colors = 0
# for skin_file in all_skin_files:
#     frames, current_num_colors = load_and_preprocess_data(skin_file, PIXEL_HEIGHT, PIXEL_WIDTH)
#     if frames.size > 0:
#         all_preprocessed_frames.append(frames)
#         max_num_colors = max(max_num_colors, current_num_colors)
# if all_preprocessed_frames:
#     x_train = np.concatenate(all_preprocessed_frames, axis=0)
#     NUM_COLORS = max_num_colors # Update global NUM_COLORS based on all loaded data
# else:
#     x_train = np.array([])
# print(f"Total number of frames loaded: {x_train.shape[0]}")

# For this example, we'll just use the single provided file
try:
    x_train, NUM_COLORS = load_and_preprocess_data(
        SAMPLE_JSON_FILE_PATH, PIXEL_HEIGHT, PIXEL_WIDTH
    )
    if x_train.size == 0:
        raise ValueError("No data loaded from the sample JSON file.")
    print(f"Shape of loaded training data: {x_train.shape}")
    print(f"Dynamically determined number of colors: {NUM_COLORS}")
except Exception as e:
    print(f"Error loading data: {e}")
    print("Please ensure your data directory and file paths are correct.")
    # Create dummy data for demonstration if loading fails
    x_train = np.random.rand(100, PIXEL_HEIGHT, PIXEL_WIDTH, 1).astype(np.float32)
    NUM_COLORS = 4  # Default to 4 for dummy data
    print(f"Using dummy data of shape: {x_train.shape} and NUM_COLORS={NUM_COLORS}")


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
vae = VAE(encoder, decoder)
vae.compile(optimizer=keras.optimizers.Adam())

# --- 3. Train the VAE ---
EPOCHS = 50
BATCH_SIZE = 32

if x_train.size > 0:
    print("\nStarting VAE training...")
    vae.fit(x_train, epochs=EPOCHS, batch_size=BATCH_SIZE)
    print("VAE training complete.")
else:
    print("Cannot train VAE: No valid training data available.")

# --- 4. Generate New Skins ---


def generate_new_skin(
    vae_decoder, latent_dim, num_samples=1, num_colors_for_denormalization=4
):
    """Generates new pixel art frames using the trained VAE decoder."""
    # Sample random points from the latent space (standard normal distribution)
    random_latent_vectors = tf.random.normal(shape=(num_samples, latent_dim))
    # Decode these vectors into pixel art frames
    generated_frames_normalized = vae_decoder.predict(random_latent_vectors)

    # Denormalize pixel values back to original range and round to nearest integer
    # Use num_colors_for_denormalization to ensure correct scaling
    generated_frames_denormalized = np.round(
        generated_frames_normalized * (num_colors_for_denormalization - 1)
    )
    # Ensure values are integers and within the valid range (0 to NUM_COLORS-1)
    generated_frames_denormalized = np.clip(
        generated_frames_denormalized, 0, num_colors_for_denormalization - 1
    ).astype(int)

    return generated_frames_denormalized


# Generate a few new frames
if x_train.size > 0:
    print("\nGenerating new pixel art frames...")
    num_generations = 5
    # Pass the dynamically determined NUM_COLORS to the generation function
    new_skins = generate_new_skin(
        vae.decoder,
        LATENT_DIM,
        num_samples=num_generations,
        num_colors_for_denormalization=NUM_COLORS,
    )

    print(f"\nGenerated {num_generations} new pixel art frames:")
    for i, skin_frame in enumerate(new_skins):
        print(f"\n--- Generated Skin Frame {i+1} ---")
        # Print the 2D array representation
        for row in skin_frame.squeeze():  # .squeeze() removes the channel dimension
            print(row.tolist())  # Convert numpy array row to list for cleaner printing
else:
    print("\nSkipping generation: VAE was not trained due to lack of data.")
