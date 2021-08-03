#include <depth_camera_cmd_video.h>
#include <iostream>
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <pcl_conversions/pcl_conversions.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>

using namespace std;
int main(int argc, char **argv)
{
	DepthCameraCmdVideo cmd_video_port;
	vector<string> camera_list;
 
	// get the valid camera names
	cmd_video_port.GetDepthCameraList(camera_list);
	for (auto name : camera_list)
	{
		cout << name << endl;
	}

	if (camera_list.size() > 0)
	{
		if (cmd_video_port.Open(camera_list[0]))
		{
			string status;
			cmd_video_port.GetSystemStatus(status);
			cout << status << endl;

			//setup ros
			ros::init(argc, argv, "idc8060_pointcloud");
			ros::NodeHandle nh;

			ros::Publisher cloud_pub = nh.advertise<sensor_msgs::PointCloud2>("idc8060/pointcloud", 1);
			ros::Publisher phase_pub = nh.advertise<sensor_msgs::Image>("idc8060/phase", 1);
			ros::Publisher amplitude_pub = nh.advertise<sensor_msgs::Image>("idc8060/amplitude", 1);

			sensor_msgs::Image phase;
			phase.header.stamp = ros::Time::now();
			phase.header.frame_id = "depth";
			phase.height = cmd_video_port.GetHeight();
			phase.width = cmd_video_port.GetWidth();
			phase.encoding = sensor_msgs::image_encodings::MONO16;
			phase.step = phase.width * 2;
			phase.is_bigendian = false;
			phase.data.resize(phase.step * phase.height);

			sensor_msgs::Image amplitude;
			amplitude.header.stamp = ros::Time::now();
			amplitude.header.frame_id = "depth";
			amplitude.height = cmd_video_port.GetHeight();
			amplitude.width = cmd_video_port.GetWidth();
			amplitude.encoding = sensor_msgs::image_encodings::MONO16;
			amplitude.step = phase.width * 2;
			amplitude.is_bigendian = false;
			amplitude.data.resize(amplitude.step * amplitude.height);

			sensor_msgs::PointCloud2 cloud_output;
			pcl::PointCloud<pcl::PointXYZI> cloud;
			cloud.width = cmd_video_port.GetWidth();
			cloud.height = cmd_video_port.GetHeight();
			cloud.points.resize(cloud.width * cloud.height);

			cloud_output.header.stamp = ros::Time::now();
			float *point_cloud = new float[cmd_video_port.GetHeight() * cmd_video_port.GetWidth() * 3];
			float scale;
			cmd_video_port.GetDepthScale(scale);
			cmd_video_port.SetDepthFrameCallback([&](const DepthFrame *df, void *param) {
				cmd_video_port.ToPointsCloud(df, point_cloud,scale);

				for (int i = 0; i < df->h * df->w * 3; i += 3)
				{
					int n = i/3;
					//you should modify this limit
					if (df->amplitude[i / 3] > 100)
					{
						cloud.points[n].x = point_cloud[i];
						cloud.points[n].y = point_cloud[i+1];
						cloud.points[n].z = point_cloud[i+2];
						cloud.points[n].intensity = df->amplitude[n];
					}
					else
					{
						cloud.points[n].x = 0;
						cloud.points[n].y = 0;
						cloud.points[n].z = 0;
						cloud.points[n].intensity = 0;
					}
				}
				
				unsigned char *p = (unsigned char *)(df->phase);
				for (auto &d : phase.data)
				{
					d = *p++;
				}

				unsigned char *am = (unsigned char *)(df->amplitude);
				for (auto &d : amplitude.data)
				{
					d = *am++;
				}

				pcl::toROSMsg(cloud, cloud_output);
				cloud_output.header.frame_id = "depth";
				phase.header.stamp = ros::Time::now();
				cloud_output.header.stamp = amplitude.header.stamp = phase.header.stamp;
				phase_pub.publish(phase);
				amplitude_pub.publish(amplitude);
				cloud_pub.publish(cloud_output);
			},
												 nullptr);
			ros::Rate loop_rate(50);
			while (ros::ok())
			{
				loop_rate.sleep();
			}

			cout << "wait port close ..." << endl;
			cmd_video_port.Close();
			delete[] point_cloud;
		}
		else
		{
			cout << "open camera failed" << endl;
		}
	}
	else
	{
		cout << "no camera found" << endl;
	}
	cout << "app shutdown" << endl;
	return 0;
}
