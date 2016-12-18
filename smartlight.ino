// #define _DEBUG;

typedef enum
{
  Free,
  Click,
  Hold  
} ButtonState;


typedef enum
{
  Off,
  OneHalfOn,
  OtherHalfOn,
  On
} LampState;

typedef enum
{
  ButtonClick,
  ButtonDoubleClick,
  ButtonLongPress,
  ButtonAllOffPress
} MessageType;

struct Message
{
  MessageType type;
  int source;
};

const unsigned long LONG_PRESS_TIME = 500;
const unsigned long POWER_OFF_PRESS_TIME = 1500;
const unsigned long MessageQueueLength = 10;

#ifdef _DEBUG
#define TRACE(x) Serial.print(x)
#define TRACELN(x) Serial.println(x)
#define TRACEI(x) Serial.print(x, DEC)
#else
#define TRACE(x)
#define TRACELN(x)
#define TRACEI(x)
#endif


class MessageQueue
{
  Message messages_[MessageQueueLength];
  int count_;
  int next_;
  
public:
  MessageQueue()
  {
    count_ = 0;
    next_ = 0;
  }
  
  bool Add(MessageType type, int source)
  {
    if (count_ > MessageQueueLength)
    {
      TRACE(F("Message queue is full"));
      return false; // Очередь переполнена
    }

    messages_[count_].type = type;
    messages_[count_].source = source;
    count_++;
  }

  void Clear()
  {
    count_ = 0;
  }

  Message* Next()
  {
    if (next_ < count_)
    {
      return messages_ + next_++;
    }

    return NULL;
  }

  void Reset()
  {
    next_ = 0;
  }

  void Dump(void)
  {
    if (count_ > 0)
    {
      TRACELN(F("Message queue dump:"));
    }
    
    for(int i = 0; i < count_; ++i)
    {
      TRACEI(messages_[i].type);
      TRACE(F(" "));
      TRACEI(messages_[i].source);
      TRACELN("");
    }
  }
};

class Button
{
public:
  int _pin;
  const char* _name; 
  ButtonState _state;
  long _clickTime;
  bool _allOffEnabled;

  Button(const char* name, int pin, bool allOffEnabled = false)
  {
    _pin = pin;
    _name = name;
    pinMode(pin, INPUT);
    digitalWrite(pin, HIGH);
    _state = Free;
    _allOffEnabled = allOffEnabled;
  }

  void Ask(MessageQueue& queue)
  {
    int val = digitalRead(_pin);
    long span;
    switch (_state)
    {
      case Free:
        if (val == LOW)
        {
          TRACE(_name); TRACELN(" button clicked");
          // кнопка только что нажата
          _state = Click;
          _clickTime = millis();
        }
        break;
      case Click:
        span = millis() - _clickTime;
        if (val == HIGH)
        {
          if (span >= LONG_PRESS_TIME && (!_allOffEnabled || span < POWER_OFF_PRESS_TIME))
          {
            // Долгое удержание кнопки
            TRACE(_name); TRACELN(" released after long press");
            if (_allOffEnabled)
            {
              queue.Add(ButtonLongPress, _pin);
            }
          }
          else
          {
            TRACE(_name); TRACELN(" button released");             
            queue.Add(ButtonClick, _pin);
          }
          _state = Free;        
        }
        else
        {
          if (_allOffEnabled && span >= POWER_OFF_PRESS_TIME)
          {
            // Долгое удержание кнопки
            TRACE(_name); TRACELN(" very long pressing");
            queue.Add(ButtonAllOffPress, 0);
            _state = Hold;
          }
          else if (!_allOffEnabled && span >= LONG_PRESS_TIME)
          {
            queue.Add(ButtonLongPress, _pin); 
            _state = Hold;     
          }
        }
        break;
     case Hold:
       if (val == HIGH)
       {
         TRACE(_name); TRACELN(" released after long pressing");
         _state = Free;
       }
     }        
  }
  
  
};

class Lamp
{
  int _pin;
  int _pin2;
  const char* _name; 
  LampState _state;
  int _messageSource;

public:
  Lamp(const char* name, int messageSource, int pin1, int pin2 = -1)
  {
    _name = name;
    _pin = pin1;
    _pin2 = pin2;
    _messageSource = messageSource;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);   
    if (_pin2 >= 0)
    {
      pinMode(_pin2, OUTPUT);
      digitalWrite(_pin2, LOW);        
    }
  }

  void Process(MessageQueue& mq)
  {
    mq.Reset();
    Message* pMessage;
    LampState oldState = _state;
    while ((pMessage = mq.Next()) != NULL)
    {
      if (pMessage->type == ButtonAllOffPress)
      {
        _state = Off;
        break;
      }
      
      if (pMessage->source != _messageSource)
      {
        continue;
      }
        
      if (pMessage->type == ButtonClick)
      {
        switch(_state)
        {
          case Off:
            _state = On;
            break;
          default:
           _state = Off;
        }
      }

      if (_pin2 >= 0 && pMessage->type == ButtonLongPress)
      {
        if (_state == OneHalfOn)
        {
          _state = OtherHalfOn;
        }
        else
        {
          _state = OneHalfOn;
        }
      }
    }

    if (oldState != _state)
    {
      TRACE(_name); TRACE(" "); TRACEI(oldState); TRACE(" -> "); TRACEI(_state); TRACELN("");
      switch(_state)
      {
        case Off:
        case OneHalfOn:
          digitalWrite(_pin, LOW);   
          break;
        case OtherHalfOn:
        case On:
          digitalWrite(_pin, HIGH);
          break;
      }

      if (_pin2 >= 0)
      {
        switch(_state)
        {
          case Off:
          case OtherHalfOn:
            digitalWrite(_pin2, LOW);   
            break;
          case OneHalfOn:
          case On:
            digitalWrite(_pin2, HIGH);
            break;
        } 
      }
    }
  }
};

MessageQueue mq;

Button kitchenButton("kitchen", 22);
Button livingRoomButton("living room", 24);
Button roomNordButton("room nord", 26);
Button roomSouthButton("room south", 28);
Button lobbyButton("lobby", 30, true);
Button kitchenTableButton("kitchen table light", 32);
Button wcButton("wc", 34);
Button bathRoomButton("bathroom", 36);

Lamp kitchen("kitchen", 22, 49, 23);
Lamp livingRoom("living room", 24, 27, 25);
Lamp roomNord("room nord", 26, 29, 31);
Lamp roomSouth("room south", 28, 33, 35);
Lamp lobby("lobby", 30, 47, 37);
Lamp kitchenTable("kitchen table", 32, 39);
Lamp wc("wc", 34, 41);
Lamp bathRoom("bathroom", 36, 43);

void setup()
{
  Serial.begin(9600);
}



void loop()
{
  kitchenButton.Ask(mq);
  livingRoomButton.Ask(mq);
  roomNordButton.Ask(mq);
  roomSouthButton.Ask(mq);
  kitchenTableButton.Ask(mq);
  wcButton.Ask(mq);
  bathRoomButton.Ask(mq);
  lobbyButton.Ask(mq); 

  kitchen.Process(mq);
  livingRoom.Process(mq);
  roomNord.Process(mq);
  roomSouth.Process(mq);
  lobby.Process(mq);
  kitchenTable.Process(mq);
  wc.Process(mq);
  bathRoom.Process(mq);
    
  delay(25);
  mq.Clear();
}
