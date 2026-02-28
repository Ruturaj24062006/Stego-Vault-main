# Use a standard Linux environment with GCC
FROM gcc:latest

# Set working directory
WORKDIR /app

# Copy all backend files
COPY . .

# Compile without the -lws2_32 flag (Linux doesn't need it)
# We use -pthread because Linux networking often requires thread support
RUN g++ main.cpp -o stego_server -std=c++17 -pthread -O3

# Expose the port
EXPOSE 8080

# Start the server
CMD ["./stego_server"]