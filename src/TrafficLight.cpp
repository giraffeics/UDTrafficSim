#include <iostream>
#include <random>
#include <chrono>
#include "TrafficLight.h"

/* Implementation of class "MessageQueue" */

template <typename T>
T MessageQueue<T>::receive()
{
    // FP.5a : The method receive should use std::unique_lock<std::mutex> and _condition.wait() 
    // to wait for and receive new messages and pull them from the queue using move semantics. 
    // The received object should then be returned by the receive function. 
    std::unique_lock<std::mutex> lck(_mtx);
    _cnd.wait(lck, [this] {return !_queue.empty();});

    T ret = _queue.front();
    _queue.pop_front();
    return ret;
}

template <typename T>
void MessageQueue<T>::send(T &&msg)
{
    // FP.4a : The method send should use the mechanisms std::lock_guard<std::mutex> 
    // as well as _condition.notify_one() to add a new message to the queue and afterwards send a notification.
    std::lock_guard<std::mutex> lck(_mtx);

    _queue.emplace_back(msg);
    _cnd.notify_one();
}

/* Implementation of class "CrossThreadRandom" */

double CrossThreadRandom::getUniformReal(double min, double max)
{
    std::lock_guard<std::mutex> lck(_mtx);
    return std::uniform_real_distribution<double>(min, max)(_rndEngine);
}

/* Implementation of class "TrafficLight" */

CrossThreadRandom TrafficLight::_random;

TrafficLight::TrafficLight()
{
    _currentPhase = TrafficLightPhase::red;
}

void TrafficLight::waitForGreen()
{
    // FP.5b : add the implementation of the method waitForGreen, in which an infinite while-loop 
    // runs and repeatedly calls the receive function on the message queue. 
    // Once it receives TrafficLightPhase::green, the method returns.
    std::unique_lock<std::mutex> lck(_mutex);
    if(_currentPhase == green)
        return;
    lck.unlock();

    while(_phaseQueue.receive() != green){}
}

TrafficLightPhase TrafficLight::getCurrentPhase()
{
    return _currentPhase;
}

void TrafficLight::simulate()
{
    // FP.2b : Finally, the private method „cycleThroughPhases“ should be started in a thread when the public method „simulate“ is called. To do this, use the thread queue in the base class. 
    threads.emplace_back(std::thread(&TrafficLight::cycleThroughPhases, this));
}

// virtual function which is executed in a thread
void TrafficLight::cycleThroughPhases()
{
    // FP.2a : Implement the function with an infinite loop that measures the time between two loop cycles 
    // and toggles the current phase of the traffic light between red and green and sends an update method 
    // to the message queue using move semantics. The cycle duration should be a random value between 4 and 6 seconds. 
    // Also, the while-loop should use std::this_thread::sleep_for to wait 1ms between two cycles. 
    using namespace std::chrono;

    duration<int, std::milli> sleepDuration(1);

    auto tNextChange = steady_clock::now() + duration<double>(_random.getUniformReal(4.0, 6.0));

    while(true)
    {
        // SleepDuration is initialized ahead of time for efficient pass-by-reference
        std::this_thread::sleep_for(sleepDuration);

        if(steady_clock::now() >= tNextChange)
        {
            {   // change light phase in a block to create scope for the lock
                std::lock_guard<std::mutex> lightChangeLock(_mutex);
                if(_currentPhase == green)
                    _currentPhase = red;
                else
                    _currentPhase = green;

                auto msgPhase = _currentPhase;
                _phaseQueue.send(std::move(msgPhase));
            }

            // outside of lock scope, choose next phase change time
            tNextChange = steady_clock::now() + duration<double>(_random.getUniformReal(4.0, 6.0));
        }
    }
}