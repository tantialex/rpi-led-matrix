// gcc -O3 -Wall -lrpihub75_gpu example.c -o example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <rpihub75/rpihub75.h>
#include <rpihub75/util.h>
#include <rpihub75/gpu.h>
#include <rpihub75/pixels.h>
#include <sys/syscall.h>

#define ARTNET_PORT 6454
#define MAX_PACKET_SIZE 530
#define MAX_DMX_CHANNELS 510

// The structure that will hold the scene info
scene_info *scene = NULL;
uint8_t *dmxData = NULL;
bool *change = true;

// Atomic counter to track the total packets received
atomic_size_t total_packets_received = 0;

// Function to handle incoming Art-Net packets
void ProcessArtNetPacket(const uint8_t *buffer, ssize_t length)
{
    if (length < 18)
    {
        fprintf(stderr, "Packet too short for Art-Net  %ld \n", length);
        return;
    }

    // Verify the Art-Net header
    if (memcmp(buffer, "Art-Net\0", 8) != 0)
    {
        fprintf(stderr, "Not an Art-Net packet\n");
        return;
    }

    uint16_t opCode = buffer[8] | (buffer[9] << 8);
    if (opCode != 0x5000)
    { // ArtDmx opcode
        fprintf(stderr, "Not an ArtDmx packet\n");
        return;
    }

    uint16_t sequence = buffer[12];
    // Sequence number, physical port, universe, and data length
    uint16_t universe = buffer[14] | (buffer[15] << 8);

    // DMX data starts at offset 18
    const uint8_t *newDmxData = &buffer[18];

    // Copy the DMX data to the global array
    memcpy(&dmxData[universe * MAX_DMX_CHANNELS], newDmxData, MAX_DMX_CHANNELS);

    // Increment the global byte counter
    atomic_fetch_add(&total_packets_received, 1);
}

// Thread function to measure bandwidth every second
void *bandwidth_monitor(void *arg)
{
    pthread_setname_np(pthread_self(), "bandwidth-monitor");

    while (1)
    {
        // Sleep for 1 second
        sleep(1);

        // Get the total packets received and reset the counter
        size_t packets = atomic_exchange(&total_packets_received, 0);

        // Calculate bandwidth in packets per second (pps)
        double bandwidth_pps = packets;

        // Print the bandwidth
        printf("Inbound Bandwidth: %.2f pps\n", bandwidth_pps);
    }

    return NULL;
}

// Thread function to receive Art-Net packets
void *artnet_receiver(void *arg)
{
    int sockfd;
    struct sockaddr_in server_addr;

    pthread_setname_np(pthread_self(), "artnet-receiver");

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return NULL;
    }

    // Bind the socket to the Art-Net port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(ARTNET_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        return NULL;
    }

    printf("Listening for Art-Net packets on port %d...\n", ARTNET_PORT);

    uint8_t buffer[MAX_PACKET_SIZE];
    while (1)
    {
        ssize_t length = recv(sockfd, buffer, sizeof(buffer), 0);
        if (length < 0)
        {
            perror("Receive failed");
            break;
        }

        ProcessArtNetPacket(buffer, length);
        change = true;
    }

    close(sockfd);
    return NULL;
}

// Function to run the rendering loop
void *render_loop(void *arg)
{
    pthread_setname_np(pthread_self(), "render-loop");

    scene_info *scene = (scene_info *)arg;
    for (;;)
    {
        for (int i = 0; i < scene->width * scene->height; i++)
        {
            // Lock the mutex before accessing the DMX data
            int red = dmxData[i * 3], green = dmxData[i * 3 + 1], blue = dmxData[i * 3 + 2];
            RGB color = {red, green, blue};
            hub_pixel(scene, i % scene->width, i / scene->width, color);
        }
        scene->bcm_mapper(scene, NULL);  // Render image buffer in scene; Segmentation Fault
        calculate_fps(scene->fps, true); // Update FPS
    }
}

int main(int argc, char **argv)
{
    scene = default_scene(argc, argv); // Fixed typo here
    scene->stride = 3;
    scene->panel_height = 32;

    check_scene(scene);
    // Allocate memory for dmxData (since it can't be done globally)
    dmxData = (uint8_t *)malloc(scene->width * scene->height * scene->stride);
    printf("%d\n", scene->width * scene->height * scene->stride);

    // Create threads for receiving Art-Net packets, rendering, and bandwidth monitoring
    pthread_t receiver_thread, render_thread, bandwidth_thread;

    pthread_create(&receiver_thread, NULL, artnet_receiver, NULL);
    pthread_create(&render_thread, NULL, render_loop, scene);
    pthread_create(&bandwidth_thread, NULL, bandwidth_monitor, NULL);

    render_forever(scene);

    // Wait for the threads to finish (though they run forever)
    pthread_join(receiver_thread, NULL);
    pthread_join(render_thread, NULL);
    pthread_join(bandwidth_thread, NULL);

    return 0;
}
