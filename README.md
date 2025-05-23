# **Emergency Drone Coordination System**

## **Overview**
The Emergency Drone Coordination System is a sophisticated multi-threaded client-server application designed for coordinating autonomous drone fleets during emergency response situations. The system enables efficient resource allocation by dispatching drones to survivors in disaster zones using distance-optimized algorithms and real-time communications.

![Screenshot](img/program.png)

## **Key Features**
- **🤖 AI-Driven Mission Assignment**: Optimized drone-to-survivor matching using proximity calculations
- **🌐 Client-Server Architecture**: TCP/IP-based communication with JSON messaging protocol
- **📊 Real-time Visualization**: SDL2-based graphical interface with color-coded entities
- **⚡ High Performance**: Supports 50+ simultaneous drone connections and 1000+ survivors
- **🔒 Thread Safety**: Comprehensive synchronization with mutex protection and deadlock prevention
- **📈 Performance Monitoring**: Real-time metrics tracking response times, throughput, and resource utilization

## **System Architecture**
The system is built around three main components:

### **Server Component**
The central coordination server manages the emergency response operation:
- Multi-threaded architecture for concurrent drone connections
- Advanced survivor tracking with spatial indexing
- AI controller for intelligent mission assignment
- Thread-safe data structures for reliable coordination
- Performance monitoring with real-time statistics

### **Client Component**
Autonomous drone clients connect to the server and perform rescue missions:
- TCP/IP-based communication with the coordination server
- Automatic status reporting and mission execution
- Path planning for efficient navigation
- Fault-tolerant operation with connection management

### **Visualization System**
Real-time graphical representation of the emergency response scenario:
- Color-coded display: survivors (red), idle drones (blue), active missions (green)
- Dynamic map updating as missions progress
- Performance statistics dashboard

![Load Testing with 50 drones](img/load_testing.png)

## **Technical Implementation**

### **Communication Protocol**
A lightweight JSON-based protocol enables efficient server-client interaction:
- **Drone → Server**: Status updates, mission completions
- **Server → Drone**: Mission assignments, heartbeat messages
- See [communication protocol details](communication-protocol.md) for complete specifications

### **Multi-Threading Model**
The system uses a carefully designed threading architecture:
- One thread per connected drone for message handling
- Dedicated threads for AI control, survivor generation, and performance monitoring
- Main thread handles visualization and user interaction

### **Synchronization Strategy**
Thread safety is ensured through a comprehensive synchronization approach:
- Per-list mutex protection for shared data structures
- Fine-grained locking to maximize concurrency
- Semaphore-based flow control for list operations
- Strict locking hierarchy to prevent deadlocks

---

### **Documentation Generation**
The project uses Doxygen to generate comprehensive documentation. Follow these steps to generate updated documentation:

#### **Prerequisites**
- Install Doxygen: `sudo apt-get install doxygen` (Ubuntu/Debian)

#### **Generating Documentation**
1. **Basic Generation**:
   ```bash
   # Generate from the project root
   doxygen Doxyfile
   ```
   This will create/update HTML documentation in the `docs/html` directory

2. **Viewing Documentation**:
   - HTML: Open `docs/html/index.html` in your browser

3. **Documentation Structure**:
   - **Main Page**: Overview, features, and architecture
   - **Modules**: Logical groups of related functionality
   - **Classes**: Data structure documentation with relationships
   - **Files**: Source code documentation with dependencies
   - **Examples**: Usage examples and test cases

---

### **Building and Running the System**

#### **Compilation**
1. **Build all components at once**:
   ```bash
   # Build the entire system (server, client, and tests)
   make all
   ```

2. **Build specific components**:
   ```bash
   # Build just the server component
   make drone_simulator

   # Build just the client component
   make drone_client

   # Build test utilities
   make tests
   ```

#### **Running the System**

1. **Start the coordination server**:
   ```bash
   # Launch the server (must be started first)
   ./drone_simulator
   ```
   This launches the central coordination server with SDL visualization. The server will display a map showing drones, survivors, and ongoing missions.

2. **Connect a single drone client**:
   ```bash
   # Launch a single drone client that connects to the server
   ./drone_client
   ```
   Each client will automatically connect to the server and begin accepting missions.

3. **Launch multiple drone clients simultaneously**:
   ```bash
   # Launch 50 drone clients in the background
   ./tests/launch_drones.sh
   ```
   The script creates multiple client instances that connect to the local server, enabling large-scale testing of the coordination system.

#### **Monitoring and Debugging**

- View real-time performance metrics in the terminal during server execution
- Check CSV output logs in the project directory for detailed performance analysis
- The final performance metrics in json format are written automatically in files in the project directory
- Use `Ctrl+C` to gracefully shut down the server and get final statistics

![Throughput metrics](img/throughput_metrics.png)
---
