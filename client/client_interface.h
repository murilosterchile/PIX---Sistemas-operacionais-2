#ifndef CLIENT_INTERFACE_HPP
#define CLIENT_INTERFACE_HPP

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include "client_processor.h"

class ClientProcessor;

class ClientInterface{
public:
	ClientInterface(ClientProcessor& processor);
	~ClientInterface();

	void start();
	void stop();

private:
	std::atomic<bool> running;
	std::thread input_thread;
	std::thread output_thread;

	ClientProcessor& processor;

	void input_loop();
	void output_loop();
};

#endif
