// Define the declared functions in the TCPSynScan.hpp
// To implement the TCP Syn Scanning using TCP protocol

#include "TCPSynScan.hpp"

int TCPSynScanUtil::TCPSynThreadNum;
pthread_mutex_t TCPSynScanUtil::TCPSynThreadNumMutex;
int TCPSynScanUtil::errorStatus;
pthread_mutex_t TCPSynScanUtil::errorStatusMutex;
ThreadSafeQueue<LogMessage> TCPSynScanUtil::logQueue;
pthread_mutex_t TCPSynScanUtil::logQueueMutex;

// The function for scanning on the specific port in this thread
void *TCPSynScanUtil::Thread_TCPSynHost(void *param)
{
    /**
     * The function for scanning on the specific port in this thread
     * @param param: The parameters for the scanning
     * @return: WHATEVER
     */
    // Declare the variables
    TCPSynHostThreadParam *p;
    std::string hostIP;
    int port;
    std::string localHostIP;
    int localPort;
    sockaddr_in SYNScanHostAddress; // The host address
    int synSocket;                  // The socket for the SYN scanning
    INT ret;
    int len;
    std::string logMessage;
    char *sendBuffer = NULL;       // The buffer for sending the SYN package
    char *recvBuffer = NULL;       // The buffer for receiving the SYN/ACK package
    struct pseudohdr *ptcph;       // The pseudo header for the TCP package
    struct tcphdr *tcph;           // The TCP header for the TCP package
    struct iphdr *recvIPHeader;    // The IP header for the received package
    struct tcphdr *recvTCPHeader;  // The TCP header for the received package
    std::string sourceHostIP;      // The IP address of the source host
    std::string destHostIP;        // The IP address of the destination host
    int sourcePort;                // The port of the source host
    int destPort;                  // The port of the destination host
    struct timeval waitingStartTP; // The start time point for waiting
    struct timeval waitingEndTP;   // The end time point for waiting

    // Get the parameters
    p = (TCPSynHostThreadParam *)param;
    hostIP = p->hostIP;
    port = p->port;
    localHostIP = p->localHostIP;
    localPort = p->localPort;

    // Create the syn scanning socket
    synSocket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    // Check the socket
    if (synSocket < 0)
    {
        // Error
        std::cerr << "[ERROR] Failed to create the socket for the TCP Syn Scanning on ip address " << hostIP << " and port " << port << std::endl;
        pthread_mutex_lock(&errorStatusMutex);
        errorStatus = ERROR;
        pthread_mutex_unlock(&errorStatusMutex);

        // Clear the resources
        delete p;
        close(synSocket);
        if (sendBuffer)
        {
            memset(sendBuffer, 0, MAX_BUFFERS_SIZE);
            free(sendBuffer);
        }
        if (recvBuffer)
        {
            memset(recvBuffer, 0, MAX_BUFFERS_SIZE);
            free(recvBuffer);
        }
        // Decrease the thread number
        pthread_mutex_lock(&TCPSynThreadNumMutex);
        TCPSynThreadNum--;
        pthread_mutex_unlock(&TCPSynThreadNumMutex);

        pthread_exit((void *)ERROR);
    }

    // Set the host address
    memset(&SYNScanHostAddress, 0, sizeof(SYNScanHostAddress));
    SYNScanHostAddress.sin_family = AF_INET;
    SYNScanHostAddress.sin_port = htons(port);
    SYNScanHostAddress.sin_addr.s_addr = inet_addr(hostIP.c_str());

    // Malloc the send and receive buffer
    sendBuffer = (char *)malloc(sizeof(struct pseudohdr) + sizeof(struct tcphdr));
    memset(sendBuffer, 0, sizeof(struct pseudohdr) + sizeof(struct tcphdr));
    recvBuffer = (char *)malloc(MAX_BUFFERS_SIZE);
    memset(recvBuffer, 0, MAX_BUFFERS_SIZE);

    // Memset the TCP SYN Package
    ptcph = (struct pseudohdr *)sendBuffer;
    tcph = (struct tcphdr *)(sendBuffer + sizeof(struct pseudohdr));

    // Memset the psedo TCP Header
    // To calculate the checksum of the TCP package
    ptcph->saddr = inet_addr(localHostIP.c_str());
    ptcph->daddr = inet_addr(hostIP.c_str());
    ptcph->useless = 0; // To fill the 8 bits of 0
    ptcph->protocol = IPPROTO_TCP;
    ptcph->length = htons(sizeof(struct tcphdr));

    // Memset the TCP Header
    tcph->th_sport = htons(localPort); // Source port
    tcph->th_dport = htons(port);      // Destination port
    tcph->th_seq = htons(123456);      // Sequence number
    tcph->th_ack = 0;                  // Acknowledgement number
    tcph->th_x2 = 0;                   // Unused
    tcph->th_off = 5;                  // Data offset
    tcph->th_flags = TH_SYN;           // Flags to set it as SYN
    tcph->th_win = htons(65535);       // Window size
    tcph->th_sum = 0;                  // Checksum initialized to 0
    tcph->th_urp = 0;                  // Urgent pointer

    // Compute the cksum using in_cksum
    // 12(Pseudo TCP Header) + 20(TCP Header)
    tcph->th_sum = in_cksum((unsigned short *)ptcph, 20 + 12);

    // Send the SYN package without the pseudo header
    len = sendto(synSocket, tcph, 20, 0, (struct sockaddr *)&SYNScanHostAddress, sizeof(SYNScanHostAddress));

    // Check the sendto
    if (len < 0)
    {
        // Error
        std::cerr << "[ERROR] Failed to send the SYN package for the TCP Syn Scanning on ip address " << hostIP << " and port " << port << std::endl;
        pthread_mutex_lock(&errorStatusMutex);
        errorStatus = ERROR;
        pthread_mutex_unlock(&errorStatusMutex);

        // Clear the resources
        delete p;
        if (sendBuffer)
        {
            memset(sendBuffer, 0, sizeof(struct pseudohdr) + sizeof(struct tcphdr));
            free(sendBuffer);
        }
        if (recvBuffer)
        {
            memset(recvBuffer, 0, MAX_BUFFERS_SIZE);
            free(recvBuffer);
        }
        // Decrease the thread number
        pthread_mutex_lock(&TCPSynThreadNumMutex);
        TCPSynThreadNum--;
        pthread_mutex_unlock(&TCPSynThreadNumMutex);

        pthread_exit((void *)ERROR);
    }

    // Set the socket to non-block mode
    ret = fcntl(synSocket, F_SETFL, O_NONBLOCK);

    if (ret < 0)
    {
        // Error setting the socket to non-block mode
        std::cerr << "[ERROR] Failed to set the socket to non-block mode for the TCP Syn Scanning on ip address " << hostIP << " and port " << port << std::endl;
        pthread_mutex_lock(&errorStatusMutex);
        errorStatus = ERROR;
        pthread_mutex_unlock(&errorStatusMutex);

        // Clear the resources
        delete p;
        close(synSocket);
        if (sendBuffer)
        {
            memset(sendBuffer, 0, sizeof(struct pseudohdr) + sizeof(struct tcphdr));
            free(sendBuffer);
        }
        if (recvBuffer)
        {
            memset(recvBuffer, 0, MAX_BUFFERS_SIZE);
            free(recvBuffer);
        }
        // Decrease the thread number
        pthread_mutex_lock(&TCPSynThreadNumMutex);
        TCPSynThreadNum--;
        pthread_mutex_unlock(&TCPSynThreadNumMutex);

        pthread_exit((void *)ERROR);
    }

    // Busy waiting for the TCP Reply
    gettimeofday(&waitingStartTP, NULL);

    while (true)
    {
        // Take the received Package as a file descriptor, using ssize_t read (int fd, void *buf, size_t count)
        len = read(synSocket, recvBuffer, MAX_BUFFERS_SIZE);
        // Check the read results
        if (len > 0)
        {
            // If the control flow reaches here, the package is received
            /* Parse the received Package */
            // Get the IP Header and TCP Header
            recvIPHeader = (struct iphdr *)recvBuffer;
            recvTCPHeader = (struct tcphdr *)(recvBuffer + (recvIPHeader->ihl << 2));

            // Get the source and destination IP address
            sourceHostIP = inet_ntoa(*(struct in_addr *)&recvIPHeader->saddr);
            destHostIP = inet_ntoa(*(struct in_addr *)&recvIPHeader->daddr);
            // Get the source and destination port
            sourcePort = ntohs(recvTCPHeader->source);
            destPort = ntohs(recvTCPHeader->dest);

            /**
             * Check if the requirements are met:
             * (1) Check if the source/dest IP and Port matches
             * (2) Check if the package type if SYN/ACK
             */
            // Check if the source/dest IP and Port matches
            if (sourceHostIP == hostIP && destHostIP == localHostIP && sourcePort == port && destPort == localPort)
            {
                // Debug
                // std::cout << "[INFO] th_flags: 0x" << std::hex << static_cast<int>(recvTCPHeader->th_flags) << std::dec << std::endl;
                // Check if the package type if SYN/ACK
                if ((recvTCPHeader->th_flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK))
                {
                    // The port is open
                    logMessage = "[INFO] Host: " + hostIP + " Port: " + std::to_string(port) + " open!";
                    break;
                }
                else if ((recvTCPHeader->th_flags & TH_RST) == TH_RST)
                {
                    // If the received package type includes TH_RST, the port is closed
                    logMessage = "[INFO] Host: " + hostIP + " Port: " + std::to_string(port) + " closed!";
                    break;
                }
            }
        }

        // If the control flow reaches here, due to the non-block mode of the socket
        // Check the time to avoid the infinite loop
        gettimeofday(&waitingEndTP, NULL);
        if ((1000000 *
                 (waitingEndTP.tv_sec - waitingStartTP.tv_sec) +
             (waitingEndTP.tv_usec - waitingStartTP.tv_usec)) /
                1000000.0 >
            MAX_SYN_TIMEOUT)
        {
            // Timeout without receiving the SYN/ACK package
            // But due to the previous success ping results, the port is filtered by the firewall
            logMessage = "[INFO] Host: " + hostIP + " Port: " + std::to_string(port) + " filtered!";
            break;
        }
    }

    // Finish sending and reading socket SYN Package, free the resources
    delete p;
    close(synSocket);
    // Clear the resources
    if (sendBuffer)
    {
        memset(sendBuffer, 0, sizeof(struct pseudohdr) + sizeof(struct tcphdr));
        free(sendBuffer);
    }
    if (recvBuffer)
    {
        memset(recvBuffer, 0, MAX_BUFFERS_SIZE);
        free(recvBuffer);
    }

    // Push the log message to the queue
    LogMessage log = {port, logMessage};
    pthread_mutex_lock(&logQueueMutex);
    logQueue.push(log);
    pthread_mutex_unlock(&logQueueMutex);

    // Decrease the thread number
    pthread_mutex_lock(&TCPSynThreadNumMutex);
    TCPSynThreadNum--;
    pthread_mutex_unlock(&TCPSynThreadNumMutex);

    // The thread exits
    pthread_exit(NULL);
}

// The function for initializing the threads for scanning, calling upon the Thread_TCPSynHost function
void *TCPSynScanUtil::Thread_TCPSynScan(void *param)
{
    /**
     * The function for initializing the threads for scanning, calling upon the Thread_TCPSynHost function
     * @param param: The parameters for the scanning
     * @return: SUCCESS or ERROR
     */
    // Declare the variables
    std::string hostIP;
    int beginPort;
    int endPort;
    std::string localHostIP;
    int localPort;
    TCPSynThreadParam *p; // The parameters for the scanning
    int tempPort;         // The temporary port for the scanning
    INT ret;
    pthread_t loggerThreadID; // The thread ID for the logger thread
    pthread_attr_t attr;
    pthread_t childThreadID; // The thread ID for the child thread
    int scanFailureFlag;

    // Get the parameters
    p = (TCPSynThreadParam *)param;
    hostIP = p->hostIP;
    beginPort = p->beginPort;
    endPort = p->endPort;
    localHostIP = p->localHostIP;
    localPort = p->localPort;

    // Initialize the variables
    TCPSynThreadNum = 0;
    errorStatus = SUCCESS;
    scanFailureFlag = false;

    std::cout << "[INFO] Begin TCP syn scan..." << std::endl;

    // Create the log processing thread
    ret = pthread_create(&loggerThreadID, NULL, [](void *arg) -> void *
                         {
        auto* args = static_cast<std::tuple<ThreadSafeQueue<LogMessage>*, int, int>*>(arg);
        logProcessingThread(*std::get<0>(*args), std::get<1>(*args), std::get<2>(*args));
        delete args;
        return nullptr; }, new std::tuple<ThreadSafeQueue<LogMessage> *, int, int>(&logQueue, beginPort, endPort));

    if (ret != 0)
    {
        std::cerr << "[ERROR] Failed to create logger thread" << std::endl;
        delete p;
        pthread_exit((void *)ERROR);
    }

    // Loop through the range of ports
    for (tempPort = beginPort; tempPort <= endPort; tempPort++)
    {
        // Get and store in the new parameter struct
        TCPSynHostThreadParam *pSynHostParam = new TCPSynHostThreadParam();
        pSynHostParam->hostIP = hostIP;
        pSynHostParam->port = tempPort;
        pSynHostParam->localHostIP = localHostIP;
        pSynHostParam->localPort = localPort;

        // Set the thread attributes to detached
        ret = pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        // Check for attribute initialization error
        if (ret != 0)
        {
            std::cerr << "[ERROR] Failed to initialize thread attributes" << std::endl;
            delete pSynHostParam;
            scanFailureFlag = true;
            break;
        }

        // Create the child thread
        ret = pthread_create(&childThreadID, &attr, Thread_TCPSynHost, (void *)pSynHostParam);

        if (ret != 0)
        {
            std::cerr << "[ERROR] Failed to create the thread for the TCP Syn Scanning on ip address " << hostIP << " and port " << tempPort << std::endl;
            scanFailureFlag = true;
            delete pSynHostParam;
            break;
        }

        // Destroy the thread attributes
        pthread_attr_destroy(&attr);

        // Increment the thread number
        pthread_mutex_lock(&TCPSynThreadNumMutex);
        TCPSynThreadNum++;
        pthread_mutex_unlock(&TCPSynThreadNumMutex);

        // Make sure there are not too many threads
        while (TCPSynThreadNum >= 100)
            sleep(3);
    }

    // If the control reaches here, the scanning is done
    while (TCPSynThreadNum != 0)
    {
        // Check the error status
        pthread_mutex_lock(&errorStatusMutex);
        if (errorStatus == ERROR)
        {
            // Error
            std::cerr << "[ERROR] Failed to scan the TCP Syn on the ip address " << hostIP << " and port " << tempPort << std::endl;
            scanFailureFlag = true;
        }
        pthread_mutex_unlock(&errorStatusMutex);
        sleep(1);
    }

    // Wait for the log thread to finish
    pthread_join(loggerThreadID, NULL);

    // The main thread exits
    if (scanFailureFlag)
        pthread_exit((void *)ERROR);
    else
        pthread_exit((void *)SUCCESS);
}