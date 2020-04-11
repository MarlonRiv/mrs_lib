#ifndef SUBSCRIBE_HANDLER_HPP
#define SUBSCRIBE_HANDLER_HPP

#include <mrs_lib/subscribe_handler.h>
#include <mutex>

namespace mrs_lib
{
  namespace impl
  {
    
    /* SubscribeHandler_impl class //{ */
    // implements the constructor, getMsg() method and data_callback method (non-thread-safe)
    template <typename MessageType>
    class SubscribeHandler_impl
    {
      public:
        using timeout_callback_t = typename SubscribeHandler<MessageType>::timeout_callback_t;
        using message_callback_t = typename SubscribeHandler<MessageType>::message_callback_t;
        using data_callback_t = std::function<void(const typename MessageType::ConstPtr&)>;

      private:
        friend class SubscribeHandler<MessageType>;

      public:
        /* constructor //{ */
        SubscribeHandler_impl(
              const SubscribeHandlerOptions& options,
              const message_callback_t& message_callback = message_callback_t()
            )
          : m_nh(options.nh),
            m_no_message_timeout(options.no_message_timeout),
            m_topic_name(options.topic_name),
            m_node_name(options.node_name),
            m_got_data(false),
            m_new_data(false),
            m_used_data(false),
            m_last_msg_received(ros::Time::now()),
            m_timeout_callback(options.timeout_callback),
            m_message_callback(message_callback),
            m_queue_size(options.queue_size),
            m_transport_hints(options.transport_hints)
        {
          if (!m_timeout_callback)
            m_timeout_callback = std::bind(&SubscribeHandler_impl<MessageType>::default_timeout_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        
          if (options.no_message_timeout != mrs_lib::no_timeout)
            m_timeout_check_timer = m_nh.createTimer(options.no_message_timeout, &SubscribeHandler_impl<MessageType>::check_timeout, this, true /*oneshot*/, false /*autostart*/);
        
          const std::string msg = "Subscribed to topic '" + m_topic_name + "' -> '" + resolved_topic_name() + "'";
          if (m_node_name.empty())
            ROS_INFO_STREAM(msg);
          else
            ROS_INFO_STREAM("[" << m_node_name << "]: " << msg);
        }
        //}

      protected:
        /* getMsg() method //{ */
        virtual typename MessageType::ConstPtr getMsg()
        {
          m_new_data = false;
          m_used_data = true;
          return peekMsg();
        }
        //}

        /* peekMsg() method //{ */
        virtual typename MessageType::ConstPtr peekMsg()
        {
          assert(m_got_data);
          if (!m_got_data)
            ROS_ERROR("[%s]: No data received yet from topic '%s' (forgot to check hasMsg()?)! Returning empty message.", m_node_name.c_str(), resolved_topic_name().c_str());
          return m_latest_message;
        }
        //}

        /* hasMsg() method //{ */
        virtual bool hasMsg() const
        {
          return m_got_data;
        }
        //}

        /* newMsg() method //{ */
        virtual bool newMsg() const
        {
          return m_new_data;
        }
        //}

        /* usedMsg() method //{ */
        virtual bool usedMsg() const
        {
          return m_used_data;
        }
        //}

        /* data_callback() method //{ */
        virtual void data_callback(const typename MessageType::ConstPtr& msg)
        {
          m_data_callback(msg);
        }

        template<bool time_consistent=false>
        typename std::enable_if<!time_consistent, void>::type
        data_callback_impl(const typename MessageType::ConstPtr& msg)
        {
          data_callback_unchecked(msg, ros::Time::now());
        }

        template<bool time_consistent=false>
        typename std::enable_if<time_consistent, void>::type
        data_callback_impl(const typename MessageType::ConstPtr& msg)
        {
          ros::Time now = ros::Time::now(); 
          const bool time_reset = check_time_reset(now);
          const bool message_valid = !m_got_data || check_time_consistent<time_consistent>(msg);
          if (message_valid || time_reset)
          {
            if (time_reset)
              ROS_WARN("[%s]: Detected jump back in time of %f. Resetting time consistency checks.", m_node_name.c_str(), (m_last_msg_received - now).toSec());
            data_callback_unchecked(msg, now);
          } else
          {
            ROS_WARN("[%s]: New message from topic '%s' is older than the latest message, skipping it.", m_node_name.c_str(), resolved_topic_name().c_str());
          }
        }
        //}

        /* check_time_reset() method //{ */
        bool check_time_reset(const ros::Time& now)
        {
          return now < m_last_msg_received;
        }
        //}

        /* check_time_consistent() method //{ */
        template<bool time_consistent=false>
        typename std::enable_if<time_consistent, bool>::type
        check_time_consistent(const typename MessageType::ConstPtr& msg)
        {
          return msg->header.stamp >= m_latest_message->header.stamp;
        }
        //}

        /* lastMsgTime() method //{ */
        virtual ros::Time lastMsgTime() const
        {
          std::lock_guard lck(m_last_msg_received_mtx);
          return m_last_msg_received;
        };
        //}

        /* topicName() method //{ */
        virtual std::string topicName() const
        {
          return m_topic_name;
        };
        //}

        /* start() method //{ */
        virtual void start()
        {
          m_timeout_check_timer.start();
          m_sub = m_nh.subscribe(m_topic_name, m_queue_size, &SubscribeHandler_impl<MessageType>::data_callback, this, m_transport_hints);
        }
        //}

        /* stop() method //{ */
        virtual void stop()
        {
          m_timeout_check_timer.stop();
          m_sub.shutdown();
        }
        //}

      public:
        /* set_owner_ptr() method //{ */
        void set_owner_ptr(const SubscribeHandlerPtr<MessageType>& ptr)
        {
          m_ptr = ptr;
        }
        //}

        /* set_data_callback() method //{ */
        template<bool time_consistent>
        void set_data_callback()
        {
          m_data_callback = std::bind(&SubscribeHandler_impl<MessageType>::template data_callback_impl<time_consistent>, this, std::placeholders::_1);
        }
        //}

      protected:
        ros::NodeHandle m_nh;
        ros::Subscriber m_sub;
    
      protected:
        ros::Duration m_no_message_timeout;
        std::string m_topic_name;
        std::string m_node_name;
    
      protected:
        bool m_ok;
        bool m_got_data;  // whether any data was received
        bool m_new_data;  // whether new data was received since last call to get_data
        bool m_used_data; // whether get_data was successfully called at least once

      protected:
        mutable std::mutex m_last_msg_received_mtx;
        ros::Time m_last_msg_received;
        ros::Timer m_timeout_check_timer;
        timeout_callback_t m_timeout_callback;

      private:
        std::weak_ptr<SubscribeHandler<MessageType>> m_ptr;
        typename MessageType::ConstPtr m_latest_message;
        message_callback_t m_message_callback;

      public:
        data_callback_t m_data_callback;

      private:
        uint32_t m_queue_size;
        ros::TransportHints m_transport_hints;

      protected:
        /* default_timeout_callback() method //{ */
        void default_timeout_callback(const std::string& topic, const ros::Time& last_msg, const int n_pubs)
        {
          /* ROS_ERROR("Checking topic %s, delay: %.2f", m_sub.getTopic().c_str(), since_msg.toSec()); */
          ros::Duration since_msg = (ros::Time::now() - last_msg);
          const std::string msg = "Did not receive any message from topic '" + topic
                                + "' for " + std::to_string(since_msg.toSec())
                                + "s (" + std::to_string(n_pubs) + " publishers on this topic)";
          if (m_node_name.empty())
            ROS_WARN_STREAM(msg);
          else
            ROS_WARN_STREAM("[" << m_node_name << "]: " << msg);
        }
        //}
        
        /* check_timeout() method //{ */
        void check_timeout([[maybe_unused]] const ros::TimerEvent& evt)
        {
          m_timeout_check_timer.stop();
          ros::Time last_msg;
          {
            std::lock_guard lck(m_last_msg_received_mtx);
            last_msg = m_last_msg_received;
            m_ok = false;
          }
          const auto n_pubs = m_sub.getNumPublishers();
          m_timeout_check_timer.start();
          m_timeout_callback(resolved_topic_name(), last_msg, n_pubs);
        }
        //}

        /* resolved_topic_name() method //{ */
        std::string resolved_topic_name() const
        {
          std::string ret = m_sub.getTopic();
          if (ret.empty())
            ret = m_nh.resolveName(m_topic_name);
          return ret;
        }
        //}

        void process_new_message(const typename MessageType::ConstPtr& msg, const ros::Time& time)
        {
          m_timeout_check_timer.stop();
          m_latest_message = msg;
          m_new_data = true;
          m_got_data = true;
          m_last_msg_received = time;
          m_timeout_check_timer.start();
        }

        void data_callback_unchecked(const typename MessageType::ConstPtr& msg, const ros::Time& time)
        {
          process_new_message(msg, time);
          if (m_message_callback)
          {
            MessageWrapper<MessageType> wrp(msg, m_topic_name);
            m_message_callback(wrp);
            if (wrp.usedMsg())
              m_new_data = false;
          }
        }
    };
    //}

    /* SubscribeHandler_threadsafe class //{ */
    template <typename MessageType>
    class SubscribeHandler_threadsafe : public SubscribeHandler_impl<MessageType>
    {
      private:
        using impl_class_t = impl::SubscribeHandler_impl<MessageType>;

      public:
        using timeout_callback_t = typename impl_class_t::timeout_callback_t;
        using message_callback_t = typename impl_class_t::message_callback_t;

        friend class SubscribeHandler<MessageType>;

      public:
        SubscribeHandler_threadsafe(
              const SubscribeHandlerOptions& options,
              const message_callback_t& message_callback = message_callback_t()
            )
          : impl_class_t::SubscribeHandler_impl(options, message_callback)
        {
        }

      protected:
        virtual void data_callback(const typename MessageType::ConstPtr& msg) override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          impl_class_t::data_callback(msg);
        }
        virtual bool hasMsg() const override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::hasMsg();
        }
        virtual bool newMsg() const override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::newMsg();
        }
        virtual bool usedMsg() const override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::usedMsg();
        }
        virtual typename MessageType::ConstPtr getMsg() override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::getMsg();
        }
        virtual typename MessageType::ConstPtr peekMsg() override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::peekMsg();
        }
        virtual ros::Time lastMsgTime() const override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::lastMsgTime();
        };
        virtual std::string topicName() const override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::topicName();
        };
        virtual void start() override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::start();
        }
        virtual void stop() override
        {
          std::lock_guard<std::recursive_mutex> lck(m_mtx);
          return impl_class_t::stop();
        }

      private:
        mutable std::recursive_mutex m_mtx;
    };
    //}

  } // namespace impl

}

#endif // SUBSCRIBE_HANDLER_HPP
