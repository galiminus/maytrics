set :user, 'maytrics'

set :application, 'maytrics.phorque.it'
set :deploy_to,   '/home/maytrics/'
set :env_file,    './config/environments/production.env'

server "#{user}@#{application}", :app
