# Use a standard Linux environment with GCC
FROM gcc:latest

RUN apt-get update \
	&& apt-get install -y python3 python3-pip \
	&& pip3 install --no-cache-dir pillow \
	&& rm -rf /var/lib/apt/lists/*

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