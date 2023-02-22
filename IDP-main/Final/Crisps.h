#include "Class.h"

// Tunnel
#define FAST 255
#define ROTATION_DELAY 1500
#define FORWARD_DELAY 500
#define WALL_DISTANCE_CM 7.5
#define WALL_DETECTION_CM 12
#define Kp 0.2
#define Ki 0
#define Kd 0

// Block
#define IR_THRESHOLD 780
#define DENSE_THRESHOLD 11

// IR
bool leftPrevIR[3] = {1, 1, 1};
bool rightPrevIR[3] = {1, 1, 1};
bool veryLeftPrevIR[3] = {1, 1, 1};
bool veryRightPrevIR[3] = {1, 1, 1};

enum currenttask
{
  lineBeforeBlock,
  blockDensity,
  blockPickup,
  lineBlockTunnel,
  tunnel,
  lineAfterTunnel,
  rotateLeft,
  rotateRight,
  blockDropOff,
  end
};

enum Block
{
  block1,
  block2,
  block3
};

class Crisps
{
public:
  LineFollower lineFollower1; // left
  LineFollower lineFollower2; // right
  LineFollower lineFollower3; // very left
  LineFollower lineFollower4; // very right
  Motors motorL;
  Motors motorR;
  IR irBlock;
  Ultrasound *ultrasoundBlock;
  Ultrasound *ultrasoundTunnel;
  Grabber grabber;
  static const int maxSpeed = 255;
  bool onLine;
  bool firstRotation = false;

  // block
  double robotStartTime = 0;

  // tunnel
  float motorRatio = 1;
  unsigned long currentTime, previousTime;
  double elapsedTime;
  double error, lastError, cumError, rateError, out;

  // branch counter
  int leftBranch = 0;
  int rightBranch = 0;
  double prevLeftBranchTime = 0;
  double currLeftBranchTime = 0;
  double prevRightBranchTime = 0;
  double currRightBranchTime = 0;
  double prevFullBranchTime = 0;
  double currFullBranchTime = 0;
  double branchTimeTol = 1000; // 1 s

  int currentTask;
  int block;
  bool blockIsDense;
  bool blockData_bool = false;
  bool tunnelData_bool = false;
  bool blockReleased_bool = false;

  // Motor light
  double currFlash = 0;
  double prevFlash = 0;
  bool isMoving = false;
  bool lightOn = false;

  Crisps() = default;
  Crisps(Ultrasound *ultraBlock, Ultrasound *ultraTunnel) : lineFollower1(LINEFOLLOWER_1), // this is the line follower on the left for following the main line
                                                            lineFollower2(LINEFOLLOWER_2), // this is the line follower on the right for following the main line
                                                            lineFollower3(LINEFOLLOWER_3), // very left
                                                            lineFollower4(LINEFOLLOWER_4), // very right
                                                            motorL(MOTOR_L, AMBER_LIGHT),
                                                            motorR(MOTOR_R, AMBER_LIGHT),
                                                            irBlock(IR_PIN),
                                                            grabber(SERVO_1)
  {
    // Line sensor
    onLine = true;

    // Motor
    motorL.begin();
    motorR.begin();

 

    // Ultrasound
    ultrasoundBlock = ultraBlock;
    ultrasoundTunnel = ultraTunnel;

    // Block light
    pinMode(RED_LIGHT, OUTPUT);
    digitalWrite(RED_LIGHT, LOW);
    pinMode(GREEN_LIGHT, OUTPUT);
    digitalWrite(GREEN_LIGHT, LOW);

    // Button
    pinMode(BUTTON_PIN, INPUT);

    // Task
    currentTask = 0;
    block = 0;
  }

  // Flash
  void flash() 
  {
    currFlash = millis();
    if (isMoving && currFlash - prevFlash > 100) {
      // flash light
      Serial.println("Flash");
      if (!lightOn) {
        lightOn = true;
        digitalWrite(AMBER_LIGHT, HIGH);
      }
      else {
        lightOn = false;
        digitalWrite(AMBER_LIGHT, LOW);
      }

      // reset prevFlash
      prevFlash = currFlash;
    }
  }

  // Begin
  void begin()
  {
    Serial.println("Begin");
    grabber.begin();
    fullForward();
    grabber.angle(75);
    delay(2000);
    grabber.detach();
    Serial.println("Done begin");
    robotStartTime = millis();
  }

  // Pure Motion
  void fullForward()
  {
    isMoving = true;
    motorL.forward(maxSpeed);
    motorR.forward(maxSpeed);
    flash();
  }

  void fullBackward()
  {
    isMoving = true;
    motorL.backward(maxSpeed);
    motorR.backward(maxSpeed);
    flash();
  }

  void stop()
  {
    motorL.stop();
    motorR.stop();
    isMoving = false;

    // turn off light
    digitalWrite(AMBER_LIGHT, LOW);
  }

  void clockwise()
  {
    isMoving = true;
    motorL.forward(maxSpeed);
    motorR.backward(maxSpeed);
    flash();
  }

  void anticlockwise()
  {
    isMoving = true;
    motorL.backward(maxSpeed);
    motorR.forward(maxSpeed);
    flash();
  }

  void rotate180() {
    isMoving = true;
    flash();
    motorL.backward(maxSpeed);
    motorR.forward(maxSpeed);
    delay(500);
    bool leftLine = lineFollower1.getLineData();
    bool rightLine = lineFollower2.getLineData();
    do {
      flash();
    } while (!leftLine || !rightLine);
    stop();
  }

  void leftAnchoredAnticlockwise()
  {
    isMoving = true;
    motorL.stop();
    motorR.forward(maxSpeed);
    flash();
  }

  void leftAnchoredClockwise()
  {
    isMoving = true;
    motorL.stop();
    motorR.backward(maxSpeed);
    flash();
  }

  void rightAnchoredAnticlockwise()
  {
    isMoving = true;
    motorL.backward(maxSpeed);
    motorR.stop();
    flash();
  }

  void rightAnchoredClockwise()
  {
    isMoving = true;
    motorL.forward(maxSpeed);
    motorR.stop();
    flash();
  }

  // Line following motion
  void followLine()
  {
    isMoving = true;
    flash();
    int step = 60;
    bool leftLine = lineFollower1.getLineData();
    bool rightLine = lineFollower2.getLineData();
    bool veryLeftLine = lineFollower3.getLineData();
    bool veryRightLine = lineFollower4.getLineData();

    
    if (!leftLine || !rightLine)
    {
      onLine = false;
      if (!leftLine && rightLine)
      {
        int speedLeft = motorL.getSpeed();
        if (speedLeft - step < 0)
        {
          motorL.forward(0);
          motorR.forward(maxSpeed);
        }
        else
        {
          motorL.forward(speedLeft - step);
          motorR.forward(maxSpeed);
        }
      }
      else if (leftLine && !rightLine)
      {
        int speedRight = motorR.getSpeed();
        if (speedRight - step < 0)
        {
          motorR.forward(0);
          motorL.forward(maxSpeed);
        }
        else
        {
          motorR.forward(speedRight - step);
          motorL.forward(maxSpeed);
        }
      }
      else
      {
        motorL.forward(maxSpeed);
        motorR.forward(maxSpeed);
      }
    }
    else if (onLine == false)
    {
      onLine = true;
      motorL.forward(maxSpeed);
      motorR.forward(maxSpeed);
    }
    /*
    Serial.println("Line");
    Serial.println(leftLine);
    Serial.println(rightLine);
    Serial.println(veryLeftLine);
    Serial.println(veryRightLine);
    
    Serial.println("Motor");
    Serial.println(motorL.getSpeed());
    Serial.println(motorR.getSpeed());
    */
    
  }


  // Line branch detection boolean for signals
  bool boolAverage(bool currLine, bool arrLine[3])
  {
    arrLine[0] = arrLine[1];
    arrLine[1] = arrLine[2];
    arrLine[2] = currLine;

    int sum = 0;
    for (int i = 0; i < 3; i++)
    {
      sum += (int)arrLine[i];
    }
    if (sum > 1)
    {
      return 1;
    }
    return 0;
  }

  bool hasLeftBranch()
  {
    //bool leftLine = lineFollower1.getLineData();
    //bool rightLine = lineFollower2.getLineData();
    bool veryLeftLine = lineFollower3.getLineData();
    //bool veryRightLine = lineFollower4.getLineData();

    return (!boolAverage(veryLeftLine, veryLeftPrevIR) /*&& boolAverage(veryRightLine, veryRightPrevIR)*/);
  }

  bool hasRightBranch()
  {
    //bool leftLine = lineFollower1.getLineData();
    //bool rightLine = lineFollower2.getLineData();
    //bool veryLeftLine = lineFollower3.getLineData();
    bool veryRightLine = lineFollower4.getLineData();

    return (/*boolAverage(veryLeftLine, veryLeftPrevIR) && */ !boolAverage(veryRightLine, veryRightPrevIR));
  }

  bool fullBranch()
  {
    bool leftLine = lineFollower1.getLineData();
    bool rightLine = lineFollower2.getLineData();
    bool veryLeftLine = lineFollower3.getLineData();
    bool veryRightLine = lineFollower4.getLineData();
    return (!boolAverage(veryLeftLine, veryLeftPrevIR) && !boolAverage(veryRightLine, veryRightPrevIR));
  }

  bool fullFirstBranch()

  {
    bool leftLine = lineFollower1.getLineData();
    bool rightLine = lineFollower2.getLineData();
    bool veryLeftLine = lineFollower3.getLineData();
    bool veryRightLine = lineFollower4.getLineData();

    return (!leftLine || !rightLine || !veryLeftLine || !veryRightLine);
  }

  bool allBlack()
  {
    bool leftLine = lineFollower1.getLineData();
    bool rightLine = lineFollower2.getLineData();
    bool veryLeftLine = lineFollower3.getLineData();
    bool veryRightLine = lineFollower4.getLineData();

    return (boolAverage(leftLine, leftPrevIR) && boolAverage(rightLine, rightPrevIR) && boolAverage(veryLeftLine, veryLeftPrevIR) && boolAverage(veryRightLine, veryRightPrevIR));
  }

  void countBranch() // !!!! RESET COUNT TO 0 AFTER EACH LAP
  {
    if (hasRightBranch() && millis() - robotStartTime > 7000)
    {
      //Serial.println("hasRight");
      currRightBranchTime = millis();
      if (currRightBranchTime - prevRightBranchTime > branchTimeTol // not same right branch
          && hasRightBranch() // recheck
          ) 
      {                                                                                 
        rightBranch += 1;
        Serial.print("Right");
        Serial.println(rightBranch);
        prevRightBranchTime = currRightBranchTime;
      }
    }

    if (hasLeftBranch() && millis() - robotStartTime > 7000)
    {
      //Serial.println("hasLeft");
      currLeftBranchTime = millis();
      if (currLeftBranchTime - prevLeftBranchTime > branchTimeTol // not same left branch
          && hasLeftBranch() // recheck
          ) 
      {                                                                              
        leftBranch += 1;
        Serial.print("Left");
        Serial.println(leftBranch);
        prevLeftBranchTime = currLeftBranchTime;
      }
    }

  }

  // Branches and zones boolean for signals
  bool reachedGreenZone() // less dense
  {
    return (rightBranch == 2);
  }

  bool reachedStartEndZone()
  {
    return (rightBranch == 4);
  }

  bool reachedRedZone() // more dense
  {
    return (rightBranch == 4);
  }

  bool reachedFirstLeftBranch()
  {
    return (leftBranch == 1);
  }

  bool reachedSecondLeftBranch()
  {
    return (leftBranch == 3);
  }

  // Tunnel - bool might need averaging
  bool inTunnel() // detect when it is inside a tunnel -> trigger tunnelPID
  {
    return (ultrasoundTunnel->wall(WALL_DETECTION_CM));
  }

  void tunnelPID()
  {
    currentTime = millis();
    elapsedTime = (double)(currentTime - previousTime);
    error = (double)ultrasoundTunnel->distError(WALL_DISTANCE_CM);
    cumError += error * elapsedTime;
    rateError = (error - lastError) / elapsedTime;

    out = Kp * error + Ki * cumError + Kd * rateError; // too far (-ve), too close (+ve)
    motorRatio = 1 + out;                              // too far (< 1), too close (> 1)

    lastError = error;
    previousTime = currentTime;

  
    Serial.print("Error:");
    Serial.println(error);
    /*
    Serial.print("Out:");
    Serial.println(out);
    Serial.print("Ratio:");
    Serial.println(motorRatio);
    Serial.println("");
    */

    if (motorRatio > 1)
    {
      motorL.forward(FAST);
      motorR.forward(FAST / motorRatio);
    }
    else if (motorRatio > 0)
    {
      motorL.forward(FAST * motorRatio);
      motorR.forward(FAST);
    }
    else
    {
      motorL.backward(abs(FAST * motorRatio));
      motorR.forward(FAST);
    }
  }

  void triggerTunnelPID() // triggered when inTunnel, break when outTunnel
  {
    isMoving = true;
    Serial.println("PID just trigger");
    delay(500);
    while (inTunnel())
    {
      tunnelPID();
      Serial.println("PID");
    }
    Serial.println("PID after trigger");
  }

  // Block collection
  bool blockDetected()
  {
    return irBlock.obstacle(IR_THRESHOLD);
  }

  void blockBeforeGrab()
  {
    // rotate 90 deg anti-clockwise
    leftAnchoredAnticlockwise();
    delay(ROTATION_DELAY);
    bool rightLine = false;
    do
    {
      rightLine = lineFollower2.getLineData();
    } while (!rightLine);
    stop();

    // straight till detect block
    fullForward();
    while (!irBlock.obstacle(IR_THRESHOLD))
    {
    }
    stop();
  }

  bool blockDifferentiate() // detect and grab the block, return whether the block is dense or not
  {
    stop();
    bool isDense = true;
    if (ultrasoundBlock->denseBlock(DENSE_THRESHOLD))
    { // dense - red
      digitalWrite(RED_LIGHT, HIGH);
      digitalWrite(GREEN_LIGHT, LOW);
      delay(5000); // change!!
      digitalWrite(RED_LIGHT, LOW);
      Serial.println("Dense");
    }
    else
    { // not dense - green
      digitalWrite(RED_LIGHT, LOW);
      digitalWrite(GREEN_LIGHT, HIGH);
      delay(5000); // change!!
      digitalWrite(GREEN_LIGHT, LOW);
      isDense = false;
      Serial.println("Not Dense");
    }
    // grab foam
    grabber.begin();
    grabber.angle(50);
    delay(500);
    grabber.detach();
    return isDense;
  }

  void blockAfterGrab()
  {
    // rotate 90 deg clockwise
    leftAnchoredClockwise();
    delay(ROTATION_DELAY);
    bool leftLine = false;
    do
    {
      leftLine = lineFollower1.getLineData();
    } while (!leftLine);
    stop();
  }

  // Block release
  void blockBeforeRelease()
  {
    stop();
    delay(500);
    // rotate 90 deg clockwise
    rightAnchoredClockwise();
    delay(ROTATION_DELAY);
    bool leftLine = false;
    do
    {
      leftLine = lineFollower1.getLineData();
    } while (leftLine);
    stop();

    // forward for certain period
    /*
    fullForward();
    delay(FORWARD_DELAY);
    stop();
    */
  }

  void blockRelease()
  {
    stop();

    // release foam
    grabber.begin();
    grabber.angle(75);
    delay(300);
    grabber.detach();
  }

  void blockAfterRelease()
  {
    // rotate 90 deg anticlockwise
    rightAnchoredAnticlockwise();
    delay(ROTATION_DELAY);
    stop();
  }

  // beginning of program
  void start()
  {
    delay(2000);
  }

  // button to trigger robot begin
  void button()
  {
    while (digitalRead(BUTTON_PIN) == 1)
    {
    }
    delay(5);
  }

  // data collection
  void dataCollection()
  {
    // bool hasRightBranch_bool = hasRightBranch();
    // bool hasLeftBranch_bool = hasLeftBranch();
    bool blockDetected_bool = blockDetected();
    bool inTunnel_bool = inTunnel();
    bool reachedGreenZone_bool = reachedGreenZone();
    bool reachedRedZone_bool = reachedRedZone();
    countBranch();
    //Serial.println(millis() - robotStartTime);

    // detect block
    if (!blockData_bool && blockDetected_bool && millis() - robotStartTime> 7000)
    { 
      blockData_bool = true;
      currentTask = blockDensity;
      Serial.println("blockDetected_bool");
    }
    
    // detect tunnel
    else if (!tunnelData_bool && inTunnel_bool && blockData_bool)
    {
      tunnelData_bool = true;
      currentTask = tunnel;
      Serial.println("inTunnel_bool");
    }

    // detect green zone for non-dense block
    else if (!blockReleased_bool && reachedGreenZone_bool && tunnelData_bool && !blockIsDense) 
    {
      blockReleased_bool = true;
      Serial.println("reachedGreenZone_bool");
      currentTask = blockDropOff;
    }

    // detect red zone for dense block
    else if (!blockReleased_bool && reachedRedZone_bool && tunnelData_bool && blockIsDense) 
    {
      blockReleased_bool = true;
      Serial.println("reachedRedZone_bool");
      currentTask = blockDropOff;
    }
  
  }

  void task()
  {
    if (block == block1)
    {
      // Serial.println("Before currentTask");
      switch (currentTask)
      {
      case lineBeforeBlock:
        if (!firstRotation && fullFirstBranch())
        {
          // do first rot
          Serial.println("First rot start");
          firstRotation = true;
          rightAnchoredClockwise();
          delay(1000);
          bool leftLine, rightLine;
          do
          {
            leftLine = lineFollower1.getLineData();
            rightLine = lineFollower2.getLineData();
            delay(5);
          } while (leftLine && rightLine);
          Serial.println("First rot end");
        }
        followLine();
        break;
      case blockDensity:
        Serial.println("blockDensity");
        blockIsDense = blockDifferentiate();
        currentTask = blockPickup;
        break;
      case blockPickup:
        Serial.println("blockPickup");
        fullForward();
        delay(10);
        followLine();
        currentTask = lineBlockTunnel;
        break;
      case lineBlockTunnel:
        // Serial.println("lineBlockTunnel");
        followLine();
        break;
      case tunnel:
        Serial.println("Tunnel");
        digitalWrite(RED_LIGHT, HIGH);   // temporary
        digitalWrite(GREEN_LIGHT, HIGH); // temporary
        triggerTunnelPID();
        followLine();
        currentTask = lineAfterTunnel;
        break;
      case lineAfterTunnel:
        // Serial.println("lineAfterTunnel");
        digitalWrite(RED_LIGHT, LOW);   // temporary
        digitalWrite(GREEN_LIGHT, LOW); // temporary
        followLine();
        break;
      case rotateLeft:
        break;
      case rotateRight:
        break;
      case blockDropOff:
        blockBeforeRelease();
        blockRelease();
        blockAfterRelease();\
        currentTask = end;
        break;
      case end:
        stop();
      /*
        if (blockIsDense) {
          rotate180();
          int currentLeftBranch = leftBranch;
          while (leftBranch == currentLeftBranch) {
            followLine();
            dataCollection();
          }
          stop();
        }
        else {
          int currentRightBranch = rightBranch;
          while (rightBranch == currentRightBranch) {
            followLine();
            dataCollection();
          }
          stop();
        }
        */
        break;
      }
    }
    if (block == block2)
    {
      switch (currentTask)
      {
      case lineBeforeBlock:
        break;
      case blockDensity:
        break;
      case blockPickup:
        break;
      case lineBlockTunnel:
        break;
      case tunnel:
        break;
      case lineAfterTunnel:
        break;
      case rotateLeft:
        break;
      case rotateRight:
        break;
      case blockDropOff:
        break;
      }
    }
    if (block == block3)
    {
      switch (currentTask)
      {
      case lineBeforeBlock:
        break;
      case blockDensity:
        break;
      case blockPickup:
        break;
      case lineBlockTunnel:
        break;
      case tunnel:
        break;
      case lineAfterTunnel:
        break;
      case rotateLeft:
        break;
      case rotateRight:
        break;
      case blockDropOff:
        break;
      }
    }
  }
};
