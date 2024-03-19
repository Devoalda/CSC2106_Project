import pandas as pd
import os
import matplotlib.pyplot as plt

# Define the base path where the CSV files are located
base_path = 'Power_Consumption/'

file_paths = [os.path.join(dirpath, file) for dirpath, _, filenames in os.walk(base_path) for file in filenames]

# for file_path in file_paths:
#     print(file_path)

colors = ['blue', 'green', 'red', 'orange']

for file in file_paths:
    df = pd.read_csv(file)
    
    # Filter out any columns named 'Unnamed: X' where X is any number
    columns = [col for col in df.columns if not col.startswith('Unnamed')]
    
    # Determine the min and max values across all columns for dynamic y-axis scaling
    min_val = df[columns].min().min()  # Min value across all columns
    max_val = df[columns].max().max()  # Max value across all columns
    
    # Increase the max value slightly for better visualization
    min_val += (max_val - min_val) * -0.1  # Adjust the min value downwards by 10% of the range (max - min)
    max_val += (max_val - min_val) * 0.1  # Adjust the max value upwards by 10% of the range
    
    # Create a figure and a set of subplots in a 2x2 arrangement
    fig, axs = plt.subplots(2, 2, figsize=(10, 8))  # Adjusted figsize for better visualization
    
    # Flatten the axs array to easily use the enumerations from the columns
    axs = axs.flatten()
    
    for i, col in enumerate(columns):
        try:
            # Assuming 'colors' is defined earlier; otherwise, replace it or define it.
            axs[i].plot(df[col], label=col, color=colors[i])  # Example with a single color for all
            axs[i].set_title(col)
            axs[i].set_xlabel("X-axis Label")  # Update with actual label
            axs[i].set_ylabel("Y-axis Label")  # Update with actual label
            axs[i].legend()
            # Set the y-axis limits based on the calculated min and adjusted max values
            axs[i].set_ylim(min_val, max_val)
        except TypeError as e:
            print(f"Cannot plot {col} due to error: {e}")
            continue
        
    # Adjust layout for a cleaner look
    plt.tight_layout()
    
    # Show the plot with all columns from this file
    plt.show()