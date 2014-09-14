require 'spec_helper'

describe "MetricController" do
  before do
    system "redis-cli flushall"
    @pid = Process.spawn "bin/maytrics"
    sleep 2
  end

  after do
    Process.kill 'TERM', @pid
  end

  it "save a metric" do
    response = RestClient.post "#{$host}/testuser/metrics.json", {metric: "testmetric", value: 3}.to_json
    response.code.should == 201
    id = JSON.parse(response.body)['id']
    response = RestClient.get "#{$host}/testuser/metrics/#{id}.json"
    JSON.parse(response).should == {"id" => id, "metric" => "testmetric", "value" => 3}
  end

  it "should fail if metric is not json" do
    RestClient.post "#{$host}/testuser/metrics.json", {metric: "testmetric", value: 3} do |response|
      response.code.should == 400
    end
  end

  it "should fail if metric is not valid" do
    RestClient.post "#{$host}/testuser/metrics.json", {metric: "testmetric", value: -1}.to_json do |response|
      response.code.should == 400
    end

    RestClient.post "#{$host}/testuser/metrics.json", {metric: "", value: 1}.to_json do |response|
      response.code.should == 400
    end

    RestClient.post "#{$host}/testuser/metrics.json", {value: 1}.to_json do |response|
      response.code.should == 400
    end

    RestClient.post "#{$host}/testuser/metrics.json", {metric: "testmetric", value: 11}.to_json do |response|
      response.code.should == 400
    end

    RestClient.post "#{$host}/testuser/metrics.json", {metric: "testmetric"}.to_json do |response|
      response.code.should == 400
    end
  end

  it "should return 404 if metric is not found" do
    RestClient.get "#{$host}/testuser/metrics/notestmetric.json" do |response|
      response.code.should == 404
    end
  end

  it "should return [] if metrics are not found" do
    RestClient.get "#{$host}/notestuser/metrics.json" do |response|
      response.body.should == "[]"
    end
  end

  it "should save several metrics" do
    response = RestClient.post "#{$host}/testuser/metrics.json", {metric: "metric1", value: 5}.to_json
    response.code.should == 201
    id1 = JSON.parse(response.body)['id']

    response = RestClient.put "#{$host}/testuser/metrics/#{id1}.json", {metric: "metric1", value: 6}.to_json
    response.code.should == 200

    response = RestClient.post "#{$host}/testuser/metrics.json", {metric: "metric2", value: 3}.to_json
    response.code.should == 201
    id2 = JSON.parse(response.body)['id']

    response = RestClient.get "#{$host}/testuser/metrics.json"
    JSON.parse(response).should == [
                                    {
                                      "metric" => "metric1",
                                      "value" => 6,
                                      "id" => id1
                                    },
                                    {
                                      "metric" => "metric2",
                                      "value" => 3,
                                      "id" => id2
                                    }
                                   ]
  end

  it "can delete metrics" do
    response = RestClient.post "#{$host}/testuser/metrics.json", {metric: "metric1", value: 5}.to_json
    response.code.should == 201
    id1 = JSON.parse(response.body)['id']

    response = RestClient.delete "#{$host}/testuser/metrics/#{id1}.json"
    response.code.should == 200

    response = RestClient.post "#{$host}/testuser/metrics.json", {metric: "metric2", value: 3}.to_json
    response.code.should == 201
    id2 = JSON.parse(response.body)['id']

    response = RestClient.get "#{$host}/testuser/metrics.json"
    JSON.parse(response).should == [
                                    {
                                      "metric" => "metric2",
                                      "value" => 3,
                                      "id" => id2
                                    }
                                   ]
  end
end

