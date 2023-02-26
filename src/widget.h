#pragma once

#include <mutex>
#include <vector>

#include <QWidget>


class Widget : public QWidget {
  Q_OBJECT

  using points = std::vector<std::pair<std::size_t, std::size_t>>;

public:
  enum class points_generator_status {
    started,
    paused,
    finished
  };

public:
  Widget(QWidget *parent = nullptr);

private:
  std::mutex              mutex_;
  points_generator_status status_ = points_generator_status::finished;
  points                  points_;

  const std::pair<std::size_t, std::size_t> value_range_ = {0, 200};
};
