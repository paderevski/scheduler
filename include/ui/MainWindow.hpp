#pragma once

#include <QMainWindow>
#include <memory>

class PreferenceModel;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
