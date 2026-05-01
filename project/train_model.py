import pandas as pd
import numpy as np
from sklearn.linear_model import LinearRegression
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error
import joblib
import os

def train_model(csv_path):
    print(f"Loading data from {csv_path}...")
    df = pd.read_csv(csv_path, encoding='latin1')
    
    # Rename columns for convenience
    df.columns = ['timestamp', 'humidity', 'temp_f', 'temp_c']
    
    # We want to predict the next humidity value based on the previous N values
    # Let's use a window of 5 previous samples
    window_size = 5
    
    # Create features
    for i in range(1, window_size + 1):
        df[f'h_lag_{i}'] = df['humidity'].shift(i)
        df[f't_lag_{i}'] = df['temp_c'].shift(i)
        
    # Drop rows with NaN (due to shifting)
    df.dropna(inplace=True)
    
    # Features: lags of humidity and temperature
    feature_cols = [f'h_lag_{i}' for i in range(1, window_size + 1)] + \
                   [f't_lag_{1}'] # Just use the most recent temp
    
    X = df[feature_cols]
    y = df['humidity']
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, shuffle=False)
    
    # Train model
    print("Training Linear Regression model...")
    model = LinearRegression()
    model.fit(X_train, y_train)
    
    # Evaluate
    y_pred = model.predict(X_test)
    mse = mean_squared_error(y_test, y_pred)
    print(f"Model trained. Mean Squared Error: {mse:.4f}")
    
    # Print coefficients for C implementation
    print("\n--- Coefficients for C Implementation ---")
    print(f"Intercept: {model.intercept_:.6f}")
    for col, coef in zip(feature_cols, model.coef_):
        print(f"{col}_coef: {coef:.6f}")
    print("------------------------------------------\n")
    
    # Save model and metadata
    model_data = {
        'model': model,
        'window_size': window_size,
        'feature_cols': feature_cols
    }
    joblib.dump(model_data, 'humidity_model.joblib')
    print("Model saved to humidity_model.joblib")

if __name__ == "__main__":
    train_model('data_3hrs.csv')
