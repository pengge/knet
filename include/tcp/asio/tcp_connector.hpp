//***************************************************************
//	created:	2020/08/01
//	author:		wkui
//***************************************************************

#pragma once
#include <unordered_map>
#include "tcp_connection.hpp"
#include "event_worker.hpp"
namespace knet {
namespace tcp {

template <class T, class Factory = ConnectionFactory<T>, class Worker = EventWorker>
class TcpConnector final {
public:
	using TPtr = std::shared_ptr<T>;
	using WorkerPtr = std::shared_ptr<Worker>;
	using FactoryPtr = Factory*;

	TcpConnector(FactoryPtr fac = nullptr, WorkerPtr worker = nullptr) {
		factory = fac;

		if (worker) {
			user_workers.emplace_back(worker);
		} else {
			// worker = std::make_shared<Worker>();
			// user_workers.emplace_back(worker);
			// worker->start();
		}
	}
	void add_worker(WorkerPtr worker) {
		if (worker) {
			user_workers.push_back(worker);
		}
	}

	bool start(uint32_t thrds = 1, FactoryPtr fac = nullptr) {
		if (fac != nullptr) {
			factory = fac;
		}
		// nothing to do
		for (uint32_t i = 0; i < thrds; i++) {
			auto worker = std::make_shared<Worker>();
			user_workers.emplace_back(worker);
			worker->start();
		}

		return true;
	}

	void stop() {}

	void handle_timeout() {
		// for (auto &item : connections)
		//{
		//	auto conn = item.second;
		//	if (conn && !conn->is_connected())
		//	{
		//		dlog("reconnect ");
		//		conn->connect();
		//	}
		//}
	}

	bool remove_connection(uint64_t cid) {
		auto itr = connections.find(cid);
		if (itr != connections.end()) {
			connections.erase(itr);
			return true;
		}
		return false;
	}

	bool add_connection(TPtr conn, const std::string& host = "", uint16_t port = 0) {
		if (conn) {
			auto worker = this->get_worker();
			auto sock =
				std::make_shared<typename T::ConnSock>(worker->thread_id(), worker->context());
			conn->init(factory, sock);
			conn->destroyer =
				std::bind(&TcpConnector<T, Factory, Worker>::destroy, this, std::placeholders::_1);
			conn->is_passive = false;
			conn->connect(host.empty() ? conn->get_remote_host() : host,
				port == 0 ? conn->get_remote_port() : port);
			connections[conn->cid] = conn;
			return true;
		}
		return false;
	}

	template <class... Args>
	TPtr add_connection(const std::string& host, uint16_t port, Args... args) {
		auto worker = this->get_worker();
		auto sock = std::make_shared<typename T::ConnSock>(worker->thread_id(), worker->context());
		TPtr conn = nullptr;
		if (factory) {
			conn = factory->create(args...);
		} else {
			conn = std::make_shared<T>(args...);
		}

		conn->init(factory, sock);

		conn->destroyer =
			std::bind(&TcpConnector<T, Factory, Worker>::destroy, this, std::placeholders::_1);
		conn->is_passive = false;
		conn->connect(host, port);
		connections[conn->cid] = conn;
		return conn;
	}

	template <class... Args>
	TPtr add_ssl_connection(
		const std::string& host, uint16_t port, const std::string& caFile, Args... args) {
		auto worker = this->get_worker();
		auto sock =
			std::make_shared<typename T::ConnSock>(worker->thread_id(), worker->context(), caFile);
		TPtr conn = nullptr;
		if (factory) {
			conn = factory->create(args...);
		} else {
			conn = std::make_shared<T>(args...);
		}

		conn->init(factory, sock);
		conn->destroyer =
			std::bind(&TcpConnector<T, Factory, Worker>::destroy, this, std::placeholders::_1);
		conn->is_passive = false;
		conn->connect(host, port);
		connections[conn->cid] = conn;

		return conn;
	}

	void destroy(std::shared_ptr<T> conn) {
		dlog("destroy connection {}", conn->cid);
		asio::post(*conn->get_context(), [this, conn]() {
			if (conn->timerid != 0) {
				conn->stop_timer(conn->timerid);
			}
			if (factory) {
				factory->destroy(conn);
			}
		});
	}

	TPtr add_wsconnection(const std::string& host, uint16_t port, bool reconn = true,
		typename Factory::ConnInitor initor = nullptr) {
		auto worker = get_worker();
		auto sock = std::make_shared<typename T::ConnSock>(worker->thread_id(), worker->context());
		auto conn = T::create(sock);
		// conn->reconnect = reconn;
		conn->is_passive = false;
		conn->connect(host, port);
		connections[conn->cid] = conn;
		return conn;
	}

	WorkerPtr get_worker(int32_t idx = 0) {
		if (!user_workers.empty()) {
			if (idx >= 0 && idx < (int32_t)user_workers.size()) {
				return user_workers[idx];
			} else {
				worker_index = (worker_index + 1) % user_workers.size();
				return user_workers[worker_index];
			}
		} else {
			elog("not worker, please create worker");
			return nullptr;
			// auto dftWorker = std::make_shared<Worker>();
			// user_workers.push_back(dftWorker);
			// return dftWorker;
		}
	}

	// void reconnect(TPtr pConn) {
	// 	dlog("reconnect connection {}", pConn->cid);
	// 	pConn->connect();
	// }

private:
	uint32_t worker_index = 0;
	FactoryPtr factory = nullptr;

	std::vector<WorkerPtr> user_workers;

	std::unordered_map<uint64_t, TPtr> connections;
};

} // namespace tcp
} // namespace knet
