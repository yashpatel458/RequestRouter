# RequestRouter

## About

RequestRouter is a multi-server load balancing system designed to efficiently distribute client requests across multiple servers. This project leverages advanced socket programming and process management techniques in C, running on a Debian Linux OS, to ensure optimal resource utilization and enhanced system performance.

### Key Features

- **Efficient Load Balancing**: Distributes client requests evenly across multiple servers to prevent overload and ensure smooth operations.
- **Advanced Socket Programming**: Utilizes TCP/IP protocols for reliable communication between clients and servers.
- **Process Management**: Implements robust process management strategies to handle multiple requests concurrently.
- **Recursive Directory Scanning**: Refined file management functions enable recursive scanning of directories, improving search efficiency.
- **File Attribute Manipulation**: Enhanced capabilities for manipulating file attributes, boosting overall system responsiveness.

### Technologies Used

- **Programming Language**: C
- **Operating System**: Debian Linux
- **Protocols**: TCP/IP

## Getting Started

### Prerequisites

- Debian-based Linux OS
- GCC Compiler

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/RequestRouter.git
   ```
2. **Navigate to the project directory:**
   ```bash
   cd RequestRouter
   ```
3. **Compile the source code:**
   ```bash
   gcc -o request_router clientw24.c mirror1.c mirror2.c serverw24.c
   ```

### Usage

1. **Start the server:**
   ```bash
   ./serverw24
   ```
2. **Start the mirrors:**
   ```bash
   ./mirror1
   ./mirror2
   ```
3. **Connect clients to the server using the client program:**
   ```bash
   ./clientw24
   ```

### Project Structure

```
RequestRouter/
├── .vscode/
├── .DS_Store
├── clientw24.c
├── mirror1.c
├── mirror2.c
├── serverw24.c
```

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository.
2. Create a new branch: `git checkout -b feature-branch`.
3. Make your changes and commit them: `git commit -m 'Add some feature'`.
4. Push to the branch: `git push origin feature-branch`.
5. Submit a pull request.

## License

This project is licensed under the MIT License. See the `LICENSE` file for more details.

## Contact

For any questions or suggestions, please reach out to [your.email@example.com](mailto:your.email@example.com).

---

Developed by [Yash Patel](https://github.com/yashpatel458)

---

Feel free to adjust the content to better suit your project's details and structure.
