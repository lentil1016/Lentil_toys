#include"subthread_s.h"
#include"message_manager.h"
#include<log4cpp/Category.hh>
#include<sstream>

//log4cpp::Category& log= log4cpp::Category::getInstance(std::string("subthread"));
std::auto_ptr<sub_thread> sub_thread::m_instance(NULL);

sub_thread::sub_thread():buffer_size(1024),log(log4cpp::Category::getInstance(std::string("subthread")))
{
	log.debug("subthread init done");
}

sub_thread* sub_thread::get_instance()
{
	if(m_instance.get()==NULL)
		m_instance.reset(new sub_thread);
	return m_instance.get();
}

void* sub_thread::sub_sender(void* a)
{
	//get init data
	thread_struct* asyner = (thread_struct*)a;

	//get sem
	sem_wait(&(asyner->sem));

	//init buffer
	int buffer_lenth=message_manager::get_instance()->buffer_lenth;
	char buffer[buffer_lenth];
	std::ostringstream oss;

	while(1)
	{
		//read buffer from stdin
		read(asyner->pipe,buffer,buffer_lenth);
		if(send(asyner->conn, buffer, strlen(buffer), 0) <= 0)
		{
			oss.str("");
			oss<<"connection "<<asyner->conn<<" sending failed, disconnecting";
			m_instance->log.debug(oss.str().c_str());
			//release sem
			sem_post(&asyner->sem);
			break;
		}
		else if (strcmp(buffer,"exit")==0)
		{
			//on condition of disconnect
			oss.str("");
			oss<<"connection "<<asyner->conn<<" receive exit signal from server, disconnecting";
			m_instance->log.debug(oss.str().c_str());
			//release sem
			sem_post(&asyner->sem);
			break;
		}
		else
		{
			oss.str("");
			oss<<"send a message to "<<asyner->conn<<" : "<<buffer;
			m_instance->log.debug(oss.str().c_str());
			//normal condition
			//clear up the buffer
			memset(buffer,0,strlen(buffer));
		}
	}
	return NULL;
}

void* sub_thread::sub_receiver(void* a)
{
	//get init data
	thread_struct* asyner = (thread_struct*)a;

	//get sem
	sem_wait(&(asyner->sem));

	//init buffer
	int buffer_lenth=message_manager::get_instance()->buffer_lenth;
	char buffer[buffer_lenth];
	std::ostringstream oss;

	while(1)
	{
		//recv from client
		if((recv(asyner->conn, buffer, buffer_lenth,0) <= 0)||(strcmp(buffer,"exit")==0))
		{
			//on condition of other side quite or receive failed
			oss.str("");
			oss<<"connection "<<asyner->conn<<" is down";
			m_instance->log.debug(oss.str().c_str());
			sem_post(&asyner->sem);
			break;
		}
		else
		{
			//on condition of received a normal message
			oss.str("");
			oss<<"receive a message from connection "<<asyner->conn<<" : "<<buffer;
			std::cout<<buffer<<"\n[receive a message from"<<asyner->conn<<"]\n";
			m_instance->log.debug(oss.str().c_str());
			//clear up the buffer
			memset(buffer,0,strlen(buffer));
		}
	}
	return NULL;
}

int sub_thread::help()
{
	//std::cout<<"=============help=============\n"\
	//	<<"x		--kill this side\n"
	//	<<"exit		--kill both side\n"
	//	<<"others	--send normal message\n"
	//	<<"==============================\n";
	return 0;
}

//communicate with net through a connection and user message through a pipe
//manage a receiver thread and a sender thread.
void* sub_thread::rs_manager(void* conn_pipe)
{
	std::ostringstream oss;
	help();

	//create asyner
	struct thread_struct asyner;
	asyner.conn=*(int*)conn_pipe;
	asyner.pipe=*(((int*)conn_pipe)+1);
	//std::cout<<"conn = "<<asyner.conn<<std::endl;

	//init sem
	sem_init(&asyner.sem, 0, 2);

	//create thread
	pthread_t sender,receiver;
	int ret;

	//int pthread_create(
	//		pthread_t *thread,					//thread id
	//		const pthread_attr_t *attr,			//thread attribute
	//		void *(*start_routine)(void*),		//thread entrance
	//		void *arg)							//thread parameter

	ret= pthread_create(&sender, NULL, sub_sender, &asyner);
	if(ret != 0)
	{
		m_instance->log.error("pthread_create error");
		exit(1);
	}
	ret= pthread_create(&receiver, NULL, sub_receiver, &asyner);
	if(ret != 0)
	{
		m_instance->log.error("pthread_create error");
		exit(1);
	}

	//detach threads
	pthread_detach(sender);
	pthread_detach(receiver);

	m_instance->log.debug("create sender and receiver thread for subthread");

	pthread_rwlock_rdlock(message_manager::get_instance()->get_subthread_lock());
	sleep(1);
	oss<<"connection on "<<asyner.conn<<" is established";
	m_instance->log.debug(oss.str().c_str());
	//std::cout<<"thread init done"<<std::endl;
	sem_wait(&asyner.sem);

	//get sem, time to destroy threads
	sem_destroy(&asyner.sem);
	pthread_cancel(sender);
	pthread_cancel(receiver);
	close(asyner.conn);
	pthread_rwlock_unlock(message_manager::get_instance()->get_subthread_lock());
	oss.str("");
	oss<<"connection on "<<asyner.conn<<" is closed";
	m_instance->log.debug(oss.str().c_str());
	close(asyner.pipe);
	return NULL;
}
