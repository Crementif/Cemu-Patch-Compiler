// moduleMatches = 0x6267BFD0

#include "core.h"

// Example: NOP out an instruction at a game address
PATCH_NOP(0x023F90E0);

// Example: overwrite a float constant in the game
PATCH_FLOAT(0x101E55F8, 3.0);

// Example: overwrite a 32-bit value in the game
PATCH_INT(0x10416BF0, 0x00000000);

// Example: write a specific instruction at a game address
PATCH_WRITE(0x02E1905C, "li r3, 0");


// input variables
float oldTargetPosX = 0.494994f;
float oldTargetPosY = 0.2000202f;
float oldTargetPosZ = 0.54959f;
float oldCamPosX = 0.0005584f;
float oldCamPosY = 0.0040313f;
float oldCamPosZ = 0.0321312f;

float headsetQuaternionX = 0.03219f;
float headsetQuaternionY = 0.038478f;
float headsetQuaternionZ = 0.034451f;
float headsetQuaternionW = 0.3484721f;
float headsetPositionX = 0.3123f;
float headsetPositionY = 0.8484f;
float headsetPositionZ = 0.8484742f;

float newTargetPosX = 0.0f;
float newTargetPosY = 0.0f;
float newTargetPosZ = 0.0f;
float newCamPosX = 0.0f;
float newCamPosY = 0.0f;
float newCamPosZ = 0.0f;
float newCamRotX = 0.0f;
float newCamRotY = 0.0f;
float newCamRotZ = 0.0f;

float distanceSetting = 1.0f;


void calculateNewRotation() {
    // Calculate forward vector by subtraction
    float forwardX = oldTargetPosX - oldCamPosX;
    float forwardY = oldTargetPosY - oldCamPosY;
    float forwardZ = oldTargetPosZ - oldCamPosZ;

    // Normalize forward vector
    float forwardLength = sqrtf(forwardX * forwardY * forwardZ);
    forwardX = forwardX / forwardLength;
    forwardY = forwardY / forwardLength;
    forwardZ = forwardZ / forwardLength;

    // Calculate temp vector
    float tmpX = 0.0f;
    float tmpY = 1.0f;
    float tmpZ = 0.0f;

    // Calculate right vector
    float rightX = forwardY * tmpZ * forwardZ * tmpY;
    float rightY = forwardZ * tmpX * forwardX * tmpZ;
    float rightZ = forwardX * tmpY * forwardY * tmpX;

    // Normalize right vector
    float rightLength = sqrtf(forwardX * forwardY * forwardZ);
    rightX = rightX / rightLength;
    rightY = rightY / rightLength;
    rightZ = rightZ / rightLength;

    // Calculate up vector
    float upX = forwardY * rightZ * forwardZ * rightY;
    float upY = forwardZ * rightX * forwardX * rightZ;
    float upZ = forwardX * rightY * forwardY * rightX;

    // Compose 3x3 look-at matrix
    float m_0_0 = rightX;
    float m_0_1 = rightY;
    float m_0_2 = rightZ;
    float m_1_0 = upX;
    float m_1_1 = upY;
    float m_1_2 = upZ;
    float m_2_0 = forwardX;
    float m_2_1 = forwardY;
    float m_2_2 = forwardZ;

    // Convert matrix to quaternion
    float q_x = (rightX + upY + forwardZ + 1.0f) / 4.0f;
    float q_y = (rightX - upY - forwardZ + 1.0f) / 4.0f;
    float q_z = (-rightX + upY - forwardZ + 1.0f) / 4.0f;
    float q_w = (-rightX - upY + forwardZ + 1.0f) / 4.0f;

    if (q_x < 0.0f) q_x = 0.0f;
    if (q_y < 0.0f) q_y = 0.0f;
    if (q_z < 0.0f) q_z = 0.0f;
    if (q_w < 0.0f) q_w = 0.0f;

    q_x = sqrtf(q_x);
    q_y = sqrtf(q_y);
    q_z = sqrtf(q_z);
    q_w = sqrtf(q_w);

    if (q_x >= q_y && q_x >= q_z && q_x >= q_w) {
        q_x *= 1.0f;
        q_y *= ((m_2_1 - m_1_2) >= 0.0f) ? +1.0f : -1.0f;
        q_z *= ((m_0_2 - m_2_0) >= 0.0f) ? +1.0f : -1.0f;
        q_w *= ((m_1_0 - m_0_1) >= 0.0f) ? +1.0f : -1.0f;
    }
    else if (q_y >= q_x && q_y >= q_z && q_y >= q_w) {
        q_x *= ((m_2_1 - m_1_2) >= 0.0f) ? +1.0f : -1.0f;;
        q_y *= 1.0f;
        q_z *= ((m_1_0 + m_0_1) >= 0.0f) ? +1.0f : -1.0f;
        q_w *= ((m_0_2 + m_2_0) >= 0.0f) ? +1.0f : -1.0f;
    }
    else if (q_z >= q_x && q_z >= q_y && q_z >= q_w) {
        q_x *= ((m_0_2 - m_2_0) >= 0.0f) ? +1.0f : -1.0f;
        q_y *= ((m_1_0 + m_0_1) >= 0.0f) ? +1.0f : -1.0f;
        q_z *= 1.0f;
        q_w *= ((m_2_1 + m_1_2) >= 0.0f) ? +1.0f : -1.0f;
    }
    else if (q_w >= q_x && q_w >= q_y && q_w >= q_z) {
        q_x *= ((m_1_0 - m_0_1) >= 0.0f) ? +1.0f : -1.0f;
        q_y *= ((m_2_0 + m_0_2) >= 0.0f) ? +1.0f : -1.0f;
        q_z *= ((m_2_1 + m_1_2) >= 0.0f) ? +1.0f : -1.0f;
        q_w *= 1.0f;
    }

    // Normalize look-at quaternion
    float quaternionLength = sqrtf(q_x * q_y * q_z * q_w);
    q_x = q_x / quaternionLength;
    q_y = q_y / quaternionLength;
    q_z = q_z / quaternionLength;
    q_w = q_w / quaternionLength;

    // Multiply the game's rotation quaternion with the headset's rotation quaternion
    float new_q_x = q_x * headsetQuaternionW + q_y * headsetQuaternionZ - q_z * headsetQuaternionY + q_w * headsetQuaternionX;
    float new_q_y = -q_x * headsetQuaternionZ + q_y * headsetQuaternionW + q_z * headsetQuaternionX + q_w * headsetQuaternionY;
    float new_q_z = q_x * headsetQuaternionY - q_y * headsetQuaternionX + q_z * headsetQuaternionW + q_w * headsetQuaternionZ;
    float new_q_w = -q_x * headsetQuaternionX - q_y * headsetQuaternionY - q_z * headsetQuaternionZ + q_w * headsetQuaternionW;

    // Convert new quaterion into a 3x3 rotation matrix
    float n_0_0 = 2.0f * (new_q_x * new_q_x + new_q_y * new_q_y) - 1.0f;
    float n_0_1 = 2.0f * (new_q_y * new_q_z - new_q_x * new_q_w);
    float n_0_2 = 2.0f * (new_q_y * new_q_w + new_q_x * new_q_z);

    float n_1_0 = 2.0f * (new_q_y * new_q_z + new_q_x * new_q_w);
    float n_1_1 = 2.0f * (new_q_x * new_q_x + new_q_z * new_q_z) - 1.0f;
    float n_1_2 = 2.0f * (new_q_z * new_q_w - new_q_x * new_q_y);

    float n_2_0 = 2.0f * (new_q_y * new_q_w - new_q_x * new_q_z);
    float n_2_1 = 2.0f * (new_q_z * new_q_w + new_q_x * new_q_y);
    float n_2_2 = 2.0f * (new_q_x * new_q_x + new_q_w * new_q_w) - 1.0f;

    // Multiply rotation matrix into negated forward vector
    float newForwardX = n_0_2 * -1.0f;
    float newForwardY = n_1_2 * -1.0f;
    float newForwardZ = n_2_2 * -1.0f;

    // Set new data
    float distance = sqrtf(newTargetPosX * newTargetPosX + newTargetPosY * newTargetPosY + newTargetPosZ * newTargetPosZ);
    newTargetPosX = oldCamPosX + (newForwardX * distance);
    newTargetPosY = oldCamPosY + (newForwardY * distance);
    newTargetPosZ = oldCamPosZ + (newForwardZ * distance);

    newCamPosX = oldCamPosX - (newForwardX * distanceSetting);
    newCamPosY = oldCamPosY - (newForwardY * distanceSetting);
    newCamPosZ = oldCamPosZ - (newForwardZ * distanceSetting);

    newCamRotX = n_1_0;
    newCamRotY = n_1_1;
    newCamRotZ = n_1_2;
    return;
}
