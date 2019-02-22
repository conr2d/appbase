#pragma once
#include <boost/asio.hpp>

#include <queue>

namespace appbase {
// adapted from: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/example/cpp11/invocation/prioritised_handlers.cpp

struct priority {
   static constexpr int high = 100;
   static constexpr int medium = 50;
   static constexpr int low = 10;
};

class execution_priority_queue : public boost::asio::execution_context
{
public:

   template <typename Function>
   void add(int priority, const string& desc, Function function)
   {
      std::unique_ptr<queued_handler_base> handler(new queued_handler<Function>(priority, desc, std::move(function)));

      handlers_.push(std::move(handler));
   }

   void execute_all()
   {
      while (!handlers_.empty()) {
         handlers_.top()->execute();
         handlers_.pop();
      }
   }

   // more, priority, description
   std::tuple<bool, int, string> execute_highest()
   {
      int priority = 0;
      string desc;
      if( !handlers_.empty() ) {
         auto& h = handlers_.top();
         priority = h->priority();
         desc = h->description();
         h->execute();
         handlers_.pop();
      }

      return { !handlers_.empty(), priority, std::move( desc ) };
   }

   class executor
   {
   public:
      executor(execution_priority_queue& q, int p, const string& desc)
            : context_(q), priority_(p), desc_( desc )
      {
      }

      execution_priority_queue& context() const noexcept
      {
         return context_;
      }

      template <typename Function, typename Allocator>
      void dispatch(Function f, const Allocator&) const
      {
         context_.add(priority_, desc_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void post(Function f, const Allocator&) const
      {
         context_.add(priority_, desc_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void defer(Function f, const Allocator&) const
      {
         context_.add(priority_, desc_, std::move(f));
      }

      void on_work_started() const noexcept {}
      void on_work_finished() const noexcept {}

      bool operator==(const executor& other) const noexcept
      {
         return &context_ == &other.context_ && priority_ == other.priority_;
      }

      bool operator!=(const executor& other) const noexcept
      {
         return !operator==(other);
      }

   private:
      execution_priority_queue& context_;
      int priority_;
      string desc_;
   };

   template <typename Function>
   boost::asio::executor_binder<Function, executor>
   wrap(int priority, const string& desc, Function&& func)
   {
      return boost::asio::bind_executor( executor( *this, priority, desc ), std::forward<Function>(func) );
   }

private:
   class queued_handler_base
   {
   public:
      queued_handler_base(int p, const string& desc)
            : priority_(p), desc_(desc)
      {
      }

      virtual ~queued_handler_base() = default;

      virtual void execute() = 0;

      int priority() const { return priority_; }

      string description() const { return desc_; }

      friend bool operator<(const std::unique_ptr<queued_handler_base>& a,
                            const std::unique_ptr<queued_handler_base>& b) noexcept
      {
         return a->priority_ < b->priority_;
      }

   private:
      int priority_;
      string desc_;
   };

   template <typename Function>
   class queued_handler : public queued_handler_base
   {
   public:
      queued_handler(int p, const string& desc, Function f)
            : queued_handler_base(p, desc), function_(std::move(f))
      {
      }

      void execute() override
      {
         function_();
      }

   private:
      Function function_;
   };

   std::priority_queue<std::unique_ptr<queued_handler_base>, std::deque<std::unique_ptr<queued_handler_base>>> handlers_;
};

} // appbase
