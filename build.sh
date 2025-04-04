#!/bin/bash

set -e  # Exit on first error

BUILD_DIR="build"

echo "🧼 Cleaning and preparing build directory..."

# Create if not exists
if [ ! -d "$BUILD_DIR" ]; then
  echo "📁 Creating build directory..."
  mkdir "$BUILD_DIR"
else
  echo "🧹 Cleaning existing build directory..."
  find "$BUILD_DIR" -mindepth 1 -delete
fi

cd "$BUILD_DIR"

echo "⚙️ Running cmake..."
cmake ..

echo "🛠️ Building project..."
make

echo "✅ Build successful!"

