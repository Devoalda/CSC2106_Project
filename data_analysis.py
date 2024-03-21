import pandas as pd
import os
import matplotlib.pyplot as plt

base_path = 'Power_Consumption/'

file_paths = [os.path.join(dirpath, file) for dirpath, _, filenames in os.walk(base_path) for file in filenames]


colors = ['blue', 'green', 'red', 'orange']

for file in file_paths:
    df = pd.read_csv(file)
    
    columns = [col for col in df.columns if not col.startswith('Unnamed')]
    
    min_val = df[columns].min().min() 
    max_val = df[columns].max().max()  
    
    min_val += (max_val - min_val) * -0.1  
    max_val += (max_val - min_val) * 0.1 
    
    fig, axs = plt.subplots(2, 2, figsize=(10, 8))  
    
    axs = axs.flatten()
    
    for i, col in enumerate(columns):
        try:
            axs[i].plot(df[col], label=col, color=colors[i]) 
            axs[i].set_title(col)
            axs[i].set_xlabel("Energy Usage Per Second")  
            axs[i].set_ylabel("Watts (W)")  
            axs[i].legend()
            axs[i].set_ylim(min_val, max_val)
        except TypeError as e:
            print(f"Cannot plot {col} due to error: {e}")
            continue
        
    plt.tight_layout()
    plt.show()