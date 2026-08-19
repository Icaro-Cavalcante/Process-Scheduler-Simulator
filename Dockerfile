# Use an official lightweight Python image (Debian-based)
FROM python:3.12-slim

# Set environment variables
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# Install system dependencies (GCC, Make, and build tools)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set working directory inside the container
WORKDIR /app

# Copy dependency definition and install Python packages
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the entire project repository
COPY . .

# Build the C simulator binary
RUN make all

# Default command: run full simulation pipeline and generate reports (plots + tables)
CMD ["make", "reports"]
