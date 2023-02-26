#include "widget.h"

#include <chrono>
#include <thread>

#include <QChart>
#include <QChartView>
#include <QGridLayout>
#include <QPushButton>
#include <QScatterSeries>
#include <QTextEdit>
#include <QTimer>

using namespace std::literals;

#if (QT_VERSION_MAJOR == 5)
using namespace QtCharts;
#endif


Widget::Widget(QWidget *parent)
  : QWidget(parent) {
  setFont({"Arial", 14, QFont::Normal});

  auto* chart = new QChart();
  auto* series = new QScatterSeries();
  series->append(value_range_.first, value_range_.first);
  series->append(value_range_.second, value_range_.second);
  chart->addSeries(series);
  chart->createDefaultAxes();
  series->clear();

  auto* chart_view = new QChartView(chart);
  chart_view->setRenderHint(QPainter::Antialiasing);
  chart_view->setMinimumHeight(400);

  QSize button_size{160, 80};
  auto* pb_start = new QPushButton("Start");
  pb_start->setFixedSize(button_size);

  auto* pb_pause = new QPushButton("Pause");
  pb_pause->setFixedSize(button_size);
  pb_pause->setCheckable(true);

  auto* pb_stop = new QPushButton("Stop");
  pb_stop->setFixedSize(button_size);

  auto* te_output = new QTextEdit();

  auto* grid = new QGridLayout(this);
  grid->addWidget(chart_view, 0, 0,   8, 5);
  grid->addWidget(pb_start,   8, 1,   1, 1, Qt::AlignCenter);
  grid->addWidget(pb_pause,   8, 2,   1, 1, Qt::AlignCenter);
  grid->addWidget(pb_stop,    8, 3,   1, 1, Qt::AlignCenter);
  grid->addWidget(te_output,  9, 0,   3, 5);
  setLayout(grid);

  auto points_generator = [this]() {
    const size_t size = 100;
    points p_vec;
    p_vec.reserve(size);

    while (true) {
      // Генерируем новую порцию данных при условии что прошлая была отдана
      if (p_vec.empty()) {
        for (std::size_t i = 0; i < size; ++i) {
          p_vec.emplace_back(std::rand() % value_range_.second, std::rand() % value_range_.second);
        }
      }

      {
        std::unique_lock lock(mutex_);
        // Отдаем новую порцию данных при условии, что прошлая была обработана
        if (status_ == points_generator_status::started && points_.empty()) {
          points_ = std::move(p_vec);
        } else if (status_ == points_generator_status::finished) {
          break;
        }
      }

      std::this_thread::sleep_for(200ms);
    }
  };

  connect(pb_start, &QPushButton::clicked, [this, points_generator, te_output]() {
    std::unique_lock lock(mutex_);

    if (status_ == points_generator_status::finished) {
      status_ = points_generator_status::started;
      lock.unlock();

      std::thread t(points_generator);
      t.detach();
      te_output->append("Starting thread... OK");
    } else {
      lock.unlock();

      te_output->append("Warning: Thread already started");
    }
  });

  connect(pb_pause, &QPushButton::clicked, [this, pb_pause](bool st) {
    std::unique_lock lock(mutex_);

    if (status_ == points_generator_status::finished) {
      pb_pause->setChecked(false);
    } else {
      status_ = st ? points_generator_status::paused : points_generator_status::started;
    }
  });

  connect(pb_stop, &QPushButton::clicked, [this, series, pb_pause, te_output](bool st) {
    QString s;

    {
      std::unique_lock lock(mutex_);

      if (status_ != points_generator_status::finished) {
        status_ = points_generator_status::finished;
        points_.clear();
        s = "Completing thread... OK";
      } else {
        s = "Warning: Thread already finished";
      }
    }

    series->clear();
    te_output->append(s);
    pb_pause->setChecked(false);
  });

  auto* timer = new QTimer(this);

  connect(timer, &QTimer::timeout, [this, series]() {
    points p_vec;
    points_generator_status current_st;
    {
      std::unique_lock lock(mutex_);
      p_vec = std::move(points_);
      current_st = status_;
    }

    // Отрисовываем новую порцию точек на графике или блокируем отрисовку,
    // если были ли нажаты кнопки "Пауза" или "Стоп"
    if (!p_vec.empty() && current_st == points_generator_status::started) {
      series->clear();
      for (const auto& [x, y] : p_vec) {
        series->append(x, y);
      }
    }
  });

  timer->start(500ms);
}
