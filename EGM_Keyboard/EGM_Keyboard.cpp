#include "stdafx.h"
#include <WinSock2.h>
#include <iostream>
#include <fstream>
#include <conio.h>
#include "egm.pb.h" // generated by Google protoc.exe

#pragma comment(lib, "Ws2_32.lib")      // socket lib
#pragma comment(lib, "libprotobuf.lib") // protobuf lib

static int portNumber = 6510;
static unsigned int sequenceNumber = 0;
const double rad_deg = 180 / 3.1415926;

using namespace abb::egm;

struct cartesian_pose {
    double cart[3];
    double orient[3];
};

union pos {
    cartesian_pose cart_pose;
    double joints[6];
};

// Create a simple sensor message
void CreateSensorMessage(EgmSensor* pSensorMessage, pos &pos, bool &joint_cmd)
{
    EgmHeader* header = new EgmHeader();
    header->set_mtype(EgmHeader_MessageType_MSGTYPE_CORRECTION);
    header->set_seqno(sequenceNumber++);
    header->set_tm(GetTickCount());
    pSensorMessage->set_allocated_header(header);

    EgmPlanned *planned = new EgmPlanned();

    // Joint command 
    if (joint_cmd) {
        EgmJoints *pj = new EgmJoints();
        // First add desired number of joints (type: repeated)
        pj->add_joints(pos.joints[0]);
        pj->add_joints(pos.joints[1]);
        pj->add_joints(pos.joints[2]);
        pj->add_joints(pos.joints[3]);
        pj->add_joints(pos.joints[4]);
        pj->add_joints(pos.joints[5]);
        planned->set_allocated_joints(pj);
    }
 
    // Cartesian command
    else {
        EgmCartesian *pc = new EgmCartesian();
        pc->set_x(pos.cart_pose.cart[0]);
        pc->set_y(pos.cart_pose.cart[1]);
        pc->set_z(pos.cart_pose.cart[2]);

        /*EgmQuaternion *pq = new EgmQuaternion();
        pq->set_u0(1.0);
        pq->set_u1(0.0);
        pq->set_u2(0.0);
        pq->set_u3(0.0);*/

        // Euler angle
        EgmEuler *pe = new EgmEuler();
        pe->set_x(pos.cart_pose.orient[0]);
        pe->set_x(pos.cart_pose.orient[1]);
        pe->set_x(pos.cart_pose.orient[2]);

        EgmPose *pcartesian = new EgmPose();
        pcartesian->set_allocated_euler(pe);
        //pcartesian->set_allocated_orient(pq);
        pcartesian->set_allocated_pos(pc);
        planned->set_allocated_cartesian(pcartesian);
    }
    pSensorMessage->set_allocated_planned(planned);
}

// Display inbound robot message
void DisplayRobotMessage(EgmRobot *pRobotMessage)
{
    if (pRobotMessage->has_header() && pRobotMessage->header().has_seqno() && pRobotMessage->header().has_tm() && pRobotMessage->header().has_mtype())
    {
        printf("SeqNo=%d || Tm=%u || Type=%d\n", pRobotMessage->header().seqno(), pRobotMessage->header().tm(), pRobotMessage->header().mtype());
        printf("Joint = %8.2lf || %8.2lf || %8.2lf || %8.2lf || %8.2lf || %8.2lf\n", pRobotMessage->feedback().joints().joints(0)*rad_deg, pRobotMessage->feedback().joints().joints(1)*rad_deg,
            pRobotMessage->feedback().joints().joints(2)*rad_deg, pRobotMessage->feedback().joints().joints(3)*rad_deg,
            pRobotMessage->feedback().joints().joints(4)*rad_deg, pRobotMessage->feedback().joints().joints(5)*rad_deg);
    }
    else
    {
        printf("No header\n");
    }
}

int main(int argc, char** argv)
{
    SOCKET sockfd;
    int n;
    struct sockaddr_in serverAddr, clientAddr;
    int len;
    char protoMessage[1400];
    // Input joint command or Cartesian command
    bool joint_cmd = false;  
   
    /* Init winsock */
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Could not open Windows connection.\n");
        exit(0);
    }

    // create socket to listen on
    sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);

    memset(&serverAddr, sizeof(serverAddr), 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(portNumber);

    // listen on all interfaces
    int result = bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
   
    pos pos;

    std::cout << "Commands: c = set Cartesian pose command, j = set joint pose command" << std::endl;
    
    char c = _getch();
    switch (c) {
    case 'c':
        std::cout << "Please specify the Cartesian position (mm) for the robot." << std::endl;
        std::cin >> pos.cart_pose.cart[0] >> pos.cart_pose.cart[1] >> pos.cart_pose.cart[2];
        std::cout << "Please specify the Euler angle orientation (degree) for the robot." << std::endl;
        std::cin >> pos.cart_pose.orient[0] >> pos.cart_pose.orient[1] >> pos.cart_pose.orient[2];
        break;
    case 'j':
        std::cout << "Please specify the joint pose (degree) for the robot." << std::endl;
        std::cin >> pos.joints[0] >> pos.joints[1] >> pos.joints[2] >> pos.joints[3] >> pos.joints[4] >> pos.joints[5];
        joint_cmd = true;
        break;
    }

    std::string messageBuffer;
    while (!result)
    {
        // receive and display message from robot
        len = sizeof(clientAddr);
        n = recvfrom(sockfd, protoMessage, 1400, 0, (struct sockaddr *)&clientAddr, &len);
        if (n < 0)
        {
            printf("Error receive message\n");
            continue;
        }

        // parse inbound message
        EgmRobot *pRobotMessage = new EgmRobot();
        pRobotMessage->ParseFromArray(protoMessage, n);
        DisplayRobotMessage(pRobotMessage);
        delete pRobotMessage;

        // create and send a sensor message
        EgmSensor *pSensorMessage = new EgmSensor();
        CreateSensorMessage(pSensorMessage, pos, joint_cmd);
        pSensorMessage->SerializeToString(&messageBuffer);
        
        // send a message to the robot
        n = sendto(sockfd, messageBuffer.c_str(), messageBuffer.length(), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
        if (n < 0)
        {
            printf("Error send message\n");
        }
        delete pSensorMessage;
    }
    return 0;
}