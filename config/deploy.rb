require 'capistrano/ext/multistage'
require 'capistrano/foreman'

set :default_shell, :bash

set :application, "maytrics"
set :repository,  "https://github.com/phorque/maytrics.git"
set :scm, :git
set :use_sudo, false
set :normalize_asset_timestamps, false

before "deploy:restart", "deploy:compile"
after  "deploy:restart", "deploy:cleanup"

set :foreman_sudo, 'sudo'
set :foreman_upstart_path, '/etc/init/'
set :foreman_options, {
  app: application,
  log: "#{shared_path}/log",
  user: 'maytrics'
}

namespace :deploy do
  task :compile do
    top.upload(env_file, "#{current_path}/.env")
    run "cd #{current_path} && cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release && make"
    foreman.export
  end

  task :start do
  end

  task :stop do ; end
  task :restart, :roles => :app, :except => { :no_release => true } do
    foreman.restart
  end
end
